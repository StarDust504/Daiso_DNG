// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DicePhysicsRollComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// Создаёт компонент и задаёт стандартное соответствие граней локальным нормалям кубика.
UDicePhysicsRollComponent::UDicePhysicsRollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Стандартная правосторонняя раскладка. Для меша с другой ориентацией векторы настраиваются в Blueprint.
	FaceLocalNormals.Add(1, FVector::UpVector);
	FaceLocalNormals.Add(2, FVector::ForwardVector);
	FaceLocalNormals.Add(3, FVector::RightVector);
	FaceLocalNormals.Add(4, FVector::LeftVector);
	FaceLocalNormals.Add(5, FVector::BackwardVector);
	FaceLocalNormals.Add(6, FVector::DownVector);
}

// При запуске игры заранее находит физическое тело кубика, если включён автоматический поиск.
void UDicePhysicsRollComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindDiceBody)
	{
		ResolveDiceBody();
	}
}

// Перед уничтожением компонента безопасно прекращает активный бросок и снимает привязки событий.
void UDicePhysicsRollComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelRoll(false);
	Super::EndPlay(EndPlayReason);
}

// Перенаправляет обновление кадра в обработчик текущего состояния броска.
void UDicePhysicsRollComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (RollState)
	{
	case ERollState::Simulating:
		UpdateSimulation(DeltaTime);
		break;
	case ERollState::FinalAlign:
		UpdateFinalAlignment(DeltaTime);
		break;
	default:
		break;
	}
}

// Запускает физический бросок к значению, используя локальную нормаль соответствующей грани.
bool UDicePhysicsRollComponent::RollToValue(const int32 Result)
{
	const FVector* FaceNormal = FaceLocalNormals.Find(Result);
	if (!FaceNormal || FaceNormal->IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Dice Physics Roll on %s has no valid local face normal for result %d."),
			*GetNameSafe(GetOwner()), Result);
		return false;
	}

	const FVector NormalizedFace = FaceNormal->GetSafeNormal();
	const FQuat FaceToUp = FQuat::FindBetweenNormals(NormalizedFace, FVector::UpVector);
	const float LandingYawRadians = bRandomizeLandingYaw
		? FMath::DegreesToRadians(FMath::FRandRange(0.0f, 360.0f))
		: 0.0f;
	const FQuat YawRotation(FVector::UpVector, LandingYawRadians);
	ActiveFaceNormalLocal = NormalizedFace;
	bHasFaceTarget = true;
	bLandingYawResolved = !bPreserveNaturalLandingYaw;
	return StartRoll(Result, (YawRotation * FaceToUp).GetNormalized());
}

// Запускает физический бросок к явно заданному мировому повороту приземления.
bool UDicePhysicsRollComponent::RollToRotation(const int32 Result, const FRotator LandingRotation)
{
	FQuat DesiredRotation = LandingRotation.Quaternion();
	ActiveFaceNormalLocal = DesiredRotation.Inverse().RotateVector(FVector::UpVector).GetSafeNormal();
	bHasFaceTarget = !ActiveFaceNormalLocal.IsNearlyZero();
	bLandingYawResolved = !bPreserveNaturalLandingYaw;
	if (!bPreserveNaturalLandingYaw && bRandomizeLandingYaw)
	{
		const FQuat RandomYaw(FVector::UpVector, FMath::DegreesToRadians(FMath::FRandRange(0.0f, 360.0f)));
		DesiredRotation = (RandomYaw * DesiredRotation).GetNormalized();
	}
	return StartRoll(Result, DesiredRotation);
}

// Подготавливает тело, сохраняет исходные настройки физики и прикладывает стартовые импульсы.
bool UDicePhysicsRollComponent::StartRoll(const int32 Result, const FQuat& DesiredWorldRotation)
{
	if (IsRolling())
	{
		CancelRoll(true);
	}

	UPrimitiveComponent* Body = ResolveDiceBody();
	if (!IsValid(Body))
	{
		UE_LOG(LogTemp, Warning, TEXT("Dice Physics Roll on %s could not find a PrimitiveComponent to roll."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ActiveBody = Body;
	ResolveBoardSurface();
	EnsureBoardBoundaryWalls();
	ActiveResult = Result;
	TargetWorldRotation = DesiredWorldRotation.GetNormalized();
	RollElapsed = 0.0f;
	StableElapsed = 0.0f;
	FinalAlignmentElapsed = 0.0f;
	bHasMeaningfulImpact = false;
	bHasBoardImpact = false;
	bHasBeenAirborne = false;
	CorrectionBounceCount = 0;

	OriginalAttachParent = Body->GetAttachParent();
	OriginalAttachSocket = Body->GetAttachSocketName();
	OriginalLinearDamping = Body->GetLinearDamping();
	OriginalAngularDamping = Body->GetAngularDamping();
	bOriginalNotifyRigidBodyCollision = Body->BodyInstance.bNotifyRigidBodyCollision;

	Body->OnComponentHit.RemoveDynamic(this, &UDicePhysicsRollComponent::HandleDiceHit);
	Body->OnComponentHit.AddDynamic(this, &UDicePhysicsRollComponent::HandleDiceHit);
	Body->SetNotifyRigidBodyCollision(true);
	if (PhysicalMaterialOverride)
	{
		Body->SetPhysMaterialOverride(PhysicalMaterialOverride);
	}

	Body->SetLinearDamping(AirLinearDamping);
	Body->SetAngularDamping(AirAngularDamping);
	Body->SetEnableGravity(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();

	const FVector HorizontalVelocity = GetBoardAwareHorizontalVelocity();
	const FVector LaunchVelocity = FVector::UpVector * UpwardSpeed + HorizontalVelocity;
	Body->AddImpulse(LaunchVelocity, NAME_None, true);

	FVector SpinAxis = FMath::VRand();
	if (FMath::Abs(SpinAxis.Z) > 0.85f)
	{
		SpinAxis = FVector(SpinAxis.X, SpinAxis.Y, SpinAxis.Z * 0.35f).GetSafeNormal();
	}
	Body->AddAngularImpulseInRadians(SpinAxis * SpinSpeed, NAME_None, true);

	RollState = ERollState::Simulating;
	SetComponentTickEnabled(true);
	OnDiceRollStarted.Broadcast(Result);
	return true;
}

// Прерывает бросок, восстанавливает физические настройки и при необходимости останавливает тело.
void UDicePhysicsRollComponent::CancelRoll(const bool bStopPhysics)
{
	if (IsValid(ActiveBody))
	{
		ActiveBody->OnComponentHit.RemoveDynamic(this, &UDicePhysicsRollComponent::HandleDiceHit);
		RestoreBodySettings();
		if (bStopPhysics)
		{
			ActiveBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
			ActiveBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			ActiveBody->SetSimulatePhysics(false);
			if (OriginalAttachParent.IsValid())
			{
				ActiveBody->AttachToComponent(OriginalAttachParent.Get(),
					FAttachmentTransformRules::KeepWorldTransform, OriginalAttachSocket);
			}
		}
	}

	RollState = ERollState::Idle;
	ActiveResult = 0;
	SetComponentTickEnabled(false);
}

// Возвращает true, пока компонент симулирует бросок или выполняет финальное выравнивание.
bool UDicePhysicsRollComponent::IsRolling() const
{
	return RollState != ERollState::Idle;
}

// Возвращает явно заданное либо первое подходящее физическое тело владельца компонента.
UPrimitiveComponent* UDicePhysicsRollComponent::ResolveDiceBody()
{
	if (IsValid(ActiveBody))
	{
		return ActiveBody;
	}

	if (AActor* Owner = GetOwner())
	{
		if (UActorComponent* ReferencedComponent = DiceBodyReference.GetComponent(Owner))
		{
			ActiveBody = Cast<UPrimitiveComponent>(ReferencedComponent);
		}

		if (!IsValid(ActiveBody) && bAutoFindDiceBody)
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
			for (UPrimitiveComponent* Candidate : PrimitiveComponents)
			{
				if (IsValid(Candidate) && Candidate->IsCollisionEnabled())
				{
					ActiveBody = Candidate;
					break;
				}
			}
		}
	}

	return ActiveBody;
}

// Возвращает телу исходные коэффициенты затухания и режим уведомлений о столкновениях.
void UDicePhysicsRollComponent::RestoreBodySettings()
{
	if (!IsValid(ActiveBody))
	{
		return;
	}

	ActiveBody->SetLinearDamping(OriginalLinearDamping);
	ActiveBody->SetAngularDamping(OriginalAngularDamping);
	ActiveBody->SetNotifyRigidBodyCollision(bOriginalNotifyRigidBodyCollision);
	ActiveBody->OnComponentHit.RemoveDynamic(this, &UDicePhysicsRollComponent::HandleDiceHit);
}
