// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/SpectacleDiceRollManager.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Dice/CPP_Dice.h"
#include "Dice/DiceImpactFeedbackBridge.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "Dice/MouseGatherDiceManager.h"
#include "Dice/NaturalDiceRollManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UnrealType.h"

namespace SpectacleDiceRoll
{
	constexpr float TwoPi = 2.0f * PI;

	const FVector MeteorSlots[] =
	{
		FVector(-0.46f, -0.28f, 0.00f),
		FVector( 0.35f,  0.38f, 0.18f),
		FVector(-0.12f,  0.52f, 0.36f),
		FVector( 0.50f, -0.18f, 0.10f),
		FVector(-0.42f,  0.22f, 0.30f),
		FVector( 0.12f, -0.50f, 0.44f),
	};
}

ASpectacleDiceRollManager::ASpectacleDiceRollManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SpectaclePhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("SpectacleDicePhysicalMaterial"));
}

void ASpectacleDiceRollManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelSpecialRoll();
	Super::EndPlay(EndPlayReason);
}

bool ASpectacleDiceRollManager::StartVortex(const bool bPredictedResult)
{
	return BeginMode(bPredictedResult
		? ESpectacleDiceMode::VortexPredicted
		: ESpectacleDiceMode::VortexNatural);
}

bool ASpectacleDiceRollManager::StartMeteors()
{
	return BeginMode(ESpectacleDiceMode::Meteors);
}

bool ASpectacleDiceRollManager::StartGravityFlip()
{
	return BeginMode(ESpectacleDiceMode::GravityFlip);
}

bool ASpectacleDiceRollManager::StartHandful()
{
	return BeginMode(ESpectacleDiceMode::Handful);
}

bool ASpectacleDiceRollManager::StartBackboard(const bool bPredictedResult)
{
	return BeginMode(bPredictedResult
		? ESpectacleDiceMode::BackboardPredicted
		: ESpectacleDiceMode::BackboardNatural);
}

bool ASpectacleDiceRollManager::StartDirectedBackboard()
{
	return BeginMode(ESpectacleDiceMode::BackboardDirected);
}

bool ASpectacleDiceRollManager::ReleaseDirectedBackboard()
{
	return ReleaseDirectedBackboardAt(GetBoardPoint(0.0f, 0.0f, 0.0f));
}

bool ASpectacleDiceRollManager::BeginMode(const ESpectacleDiceMode NewMode)
{
	if (!GetWorld() || IsActive() || IsAnotherRollActive() || NewMode == ESpectacleDiceMode::None)
	{
		return false;
	}

	TArray<ACPP_Dice*> DiceActors;
	int32 SceneDiceCount = 0;
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && IsValid(It->SMC_Dice))
		{
			++SceneDiceCount;
			if (It->GetCanRollDice() && !It->GetIsActive() && !It->bIsHidden)
			{
				DiceActors.Add(*It);
			}
		}
	}
	DiceActors.Sort([](const ACPP_Dice& Left, const ACPP_Dice& Right)
	{
		return Left.GetName() < Right.GetName();
	});
	if (DiceActors.IsEmpty())
	{
		PublishStatus(SceneDiceCount > 0
			? NSLOCTEXT("SpectacleDice", "AllDiceHeld", "Все кубики выбраны. Снимите выбор хотя бы с одного кубика.")
			: NSLOCTEXT("SpectacleDice", "NoDice", "На карте не найдены кубики."));
		return false;
	}
	LastFinishedDice.Reset();

	const bool bNeedsPrediction = NewMode == ESpectacleDiceMode::VortexPredicted
		|| NewMode == ESpectacleDiceMode::BackboardPredicted;
	if (bNeedsPrediction && DiceActors.ContainsByPredicate([](const ACPP_Dice* Dice)
		{
			return !IsValid(Dice) || !IsValid(Dice->FindComponentByClass<UDicePhysicsRollComponent>());
		}))
	{
		PublishStatus(NSLOCTEXT("SpectacleDice", "NoPredictionComponents",
			"Для режима с прогнозом не найдены физические компоненты кубиков."));
		return false;
	}

	ResolveBoardSurface();
	if (IsValid(SpectaclePhysicalMaterial))
	{
		SpectaclePhysicalMaterial->Restitution = FMath::Clamp(DiceRestitution, 0.0f, 1.0f);
		SpectaclePhysicalMaterial->Friction = FMath::Max(DiceFriction, 0.0f);
		SpectaclePhysicalMaterial->StaticFriction = FMath::Max(DiceFriction * 1.08f, 0.0f);
		SpectaclePhysicalMaterial->bOverrideRestitutionCombineMode = true;
		SpectaclePhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Max;
		SpectaclePhysicalMaterial->bOverrideFrictionCombineMode = true;
		SpectaclePhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Average;
	}

	ActiveDice.Reserve(DiceActors.Num());
	for (int32 Index = 0; Index < DiceActors.Num(); ++Index)
	{
		ACPP_Dice* Dice = DiceActors[Index];
		UPrimitiveComponent* Body = Dice->SMC_Dice;
		UDicePhysicsRollComponent* GuidedRoll = Dice->FindComponentByClass<UDicePhysicsRollComponent>();
		if (IsValid(GuidedRoll) && GuidedRoll->IsRolling())
		{
			GuidedRoll->CancelRoll(true);
		}

		FActiveDie& State = ActiveDice.AddDefaulted_GetRef();
		State.Dice = Dice;
		State.Body = Body;
		State.GuidedRoll = GuidedRoll;
		State.OriginalAttachParent = Body->GetAttachParent();
		State.OriginalAttachSocket = Body->GetAttachSocketName();
		State.OriginalPhysicalMaterial = Body->BodyInstance.GetSimplePhysicalMaterial();
		State.OriginalLinearDamping = Body->GetLinearDamping();
		State.OriginalAngularDamping = Body->GetAngularDamping();
		State.OriginalCollisionEnabled = Body->GetCollisionEnabled();
		State.OriginalWorldDynamicResponse = Body->GetCollisionResponseToChannel(ECC_WorldDynamic);
		State.bOriginalSimulatePhysics = Body->IsSimulatingPhysics();
		State.bOriginalGravity = Body->IsGravityEnabled();
		State.bOriginalNotifyRigidBodyCollision = Body->BodyInstance.bNotifyRigidBodyCollision;
		State.bOriginalUseCCD = Body->BodyInstance.bUseCCD;
		State.bOriginalCanRoll = Dice->GetCanRollDice();
		State.PhaseOffset = static_cast<float>(Index) / FMath::Max(DiceActors.Num(), 1)
			* SpectacleDiceRoll::TwoPi;
		State.SpinAxis = FVector(
			FMath::Sin(State.PhaseOffset + 0.4f),
			FMath::Cos(State.PhaseOffset * 1.31f + 0.2f),
			FMath::Sin(State.PhaseOffset * 0.71f + 1.0f)).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);

		Dice->SetCanRollDice(false);
		Dice->SetIsActive(false);
		SetGeneratedNumber(Dice, 0);
		Body->OnComponentHit.RemoveDynamic(this, &ASpectacleDiceRollManager::HandleNaturalHit);
		if (!bNeedsPrediction)
		{
			Body->OnComponentHit.AddDynamic(this, &ASpectacleDiceRollManager::HandleNaturalHit);
		}
		Body->SetNotifyRigidBodyCollision(true);
		Body->SetUseCCD(true);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Body->SetPhysMaterialOverride(SpectaclePhysicalMaterial);
		Body->SetLinearDamping(0.10f);
		Body->SetAngularDamping(0.10f);
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Body->SetEnableGravity(false);
		Body->SetSimulatePhysics(true);
		Body->WakeAllRigidBodies();
	}

	SetBoardBoundaryInset(BoardWallInset, ActiveDice[0].Body.Get());
	Mode = NewMode;
	ModeElapsed = 0.0f;
	PhaseElapsed = 0.0f;
	StableElapsed = 0.0f;
	LastImpactFeedbackCount = 0;
	FilteredGestureVelocity = FVector::ZeroVector;
	BackboardGateElapsed = 0.0f;
	bBackboardGateOpen = false;
	bGestureDragging = false;
	bWaitForActivationClickRelease = false;
	bPreviousLeftMouseDown = true;

	switch (Mode)
	{
	case ESpectacleDiceMode::VortexNatural:
	case ESpectacleDiceMode::VortexPredicted:
		Phase = ESpectacleDicePhase::Vortex;
		EffectAnchor = GetBoardPoint(0.0f, 0.0f, VortexHeight);
		GetCursorBoardPoint(VortexHeight, EffectAnchor);
		PublishStatus(Mode == ESpectacleDiceMode::VortexNatural
			? NSLOCTEXT("SpectacleDice", "VortexNatural", "Честный вихрь собирает кубики — результат ещё неизвестен…")
			: NSLOCTEXT("SpectacleDice", "VortexPredicted", "Вихрь набирает силу — грани выберутся при выпуске…"));
		break;

	case ESpectacleDiceMode::Meteors:
		Phase = ESpectacleDicePhase::MeteorDrop;
		for (int32 Index = 0; Index < ActiveDice.Num(); ++Index)
		{
			FActiveDie& State = ActiveDice[Index];
			if (UPrimitiveComponent* Body = State.Body.Get())
			{
				const FVector& Slot = SpectacleDiceRoll::MeteorSlots[
					Index % UE_ARRAY_COUNT(SpectacleDiceRoll::MeteorSlots)];
				Body->SetSimulatePhysics(false);
				Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Body->SetWorldLocation(GetBoardPoint(Slot.X, Slot.Y,
					MeteorHeight + Slot.Z * 70.0f), false, nullptr, ETeleportType::TeleportPhysics);
				Body->SetWorldRotation(FMath::VRand().Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		PublishStatus(NSLOCTEXT("SpectacleDice", "MeteorsStarted",
			"Метеоры входят в атмосферу по одному — числа определятся на доске…"));
		break;

	case ESpectacleDiceMode::GravityFlip:
		Phase = ESpectacleDicePhase::GravityLift;
		EffectAnchor = GetBoardPoint(0.0f, 0.0f, GravityCloudHeight);
		for (FActiveDie& State : ActiveDice)
		{
			if (UPrimitiveComponent* Body = State.Body.Get())
			{
				FVector Inward = EffectAnchor - Body->GetComponentLocation();
				Inward -= GetBoardUpVector() * FVector::DotProduct(Inward, GetBoardUpVector());
				Body->AddImpulse(GetBoardUpVector() * FMath::FRandRange(175.0f, 235.0f)
					+ Inward.GetClampedToMaxSize(85.0f), NAME_None, true);
				Body->AddAngularImpulseInRadians(State.SpinAxis * FMath::FRandRange(22.0f, 36.0f), NAME_None, true);
			}
		}
		PublishStatus(NSLOCTEXT("SpectacleDice", "GravityStarted",
			"Гравитация перевёрнута: кубики всплывают и собираются в облако…"));
		break;

	case ESpectacleDiceMode::Handful:
		Phase = ESpectacleDicePhase::HandfulAiming;
		EffectAnchor = GetBoardPoint(0.0f, 0.0f, HandfulHeight);
		GetCursorBoardPoint(HandfulHeight, EffectAnchor);
		PreviousHandfulAnchor = EffectAnchor;
		bWaitForActivationClickRelease = true;
		PublishStatus(NSLOCTEXT("SpectacleDice", "HandfulStarted",
			"Кубики собраны в пригоршню. Зажмите ЛКМ, проведите мышью и отпустите."));
		break;

	case ESpectacleDiceMode::BackboardNatural:
	case ESpectacleDiceMode::BackboardPredicted:
		if (!PrepareBackboardStaging())
		{
			CancelSpecialRoll();
			PublishStatus(NSLOCTEXT("SpectacleDice", "BackboardUnavailable",
				"Не удалось определить дальнюю сторону доски."));
			return false;
		}
		if (!(Mode == ESpectacleDiceMode::BackboardPredicted
			? LaunchBackboardPredicted(GetAutomaticBackboardTarget())
			: LaunchBackboardNatural(GetAutomaticBackboardTarget())))
		{
			CancelSpecialRoll();
			PublishStatus(NSLOCTEXT("SpectacleDice", "BackboardLaunchFailed",
				"Не удалось запустить кубики из-за доски."));
			return false;
		}
		break;

	case ESpectacleDiceMode::BackboardDirected:
		if (!PrepareBackboardStaging())
		{
			CancelSpecialRoll();
			PublishStatus(NSLOCTEXT("SpectacleDice", "DirectedBackboardUnavailable",
				"Не удалось подготовить направленный бросок."));
			return false;
		}
		Phase = ESpectacleDicePhase::BackboardAiming;
		bWaitForActivationClickRelease = true;
		PublishStatus(NSLOCTEXT("SpectacleDice", "DirectedBackboardStarted",
			"Кубики ждут за доской. Щёлкните ЛКМ по точке, куда их бросить."));
		break;

	default:
		CancelSpecialRoll();
		return false;
	}

	SetActorTickEnabled(true);
	return true;
}

bool ASpectacleDiceRollManager::IsAnotherRollActive() const
{
	if (!GetWorld())
	{
		return false;
	}
	for (TActorIterator<ANaturalDiceRollManager> It(GetWorld()); It; ++It)
	{
		if (It->IsRolling())
		{
			return true;
		}
	}
	for (TActorIterator<AMouseGatherDiceManager> It(GetWorld()); It; ++It)
	{
		if (It->IsInteractionActive())
		{
			return true;
		}
	}
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		const UDicePhysicsRollComponent* GuidedRoll = It->FindComponentByClass<UDicePhysicsRollComponent>();
		if (IsValid(GuidedRoll) && GuidedRoll->IsRolling())
		{
			return true;
		}
	}
	return false;
}

void ASpectacleDiceRollManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsActive())
	{
		return;
	}
	ModeElapsed += DeltaSeconds;
	UpdateBackboardGate(DeltaSeconds);
	switch (Phase)
	{
	case ESpectacleDicePhase::Vortex:
		UpdateVortex(DeltaSeconds);
		break;
	case ESpectacleDicePhase::MeteorDrop:
		UpdateMeteors(DeltaSeconds);
		break;
	case ESpectacleDicePhase::GravityLift:
		UpdateGravityFlip(DeltaSeconds);
		break;
	case ESpectacleDicePhase::HandfulAiming:
		UpdateHandful(DeltaSeconds);
		break;
	case ESpectacleDicePhase::BackboardAiming:
		UpdateBackboardAiming(DeltaSeconds);
		break;
	case ESpectacleDicePhase::NaturalSettling:
		UpdateNaturalSettling(DeltaSeconds);
		break;
	case ESpectacleDicePhase::PredictedSettling:
		UpdatePredictedSettling(DeltaSeconds);
		break;
	default:
		break;
	}
}

void ASpectacleDiceRollManager::UpdateVortex(const float DeltaSeconds)
{
	GetCursorBoardPoint(VortexHeight, EffectAnchor);
	const FVector BoardUp = GetBoardUpVector();
	const FTransform BoardTransform = IsValid(BoardSurface)
		? BoardSurface->GetComponentTransform()
		: FTransform::Identity;
	const FVector AxisX = BoardTransform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
	const FVector AxisY = BoardTransform.TransformVectorNoScale(FVector::RightVector).GetSafeNormal();
	const float RiseAlpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(ModeElapsed / 0.58f, 0.0f, 1.0f));
	for (int32 Index = 0; Index < ActiveDice.Num(); ++Index)
	{
		FActiveDie& State = ActiveDice[Index];
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		const float Angle = State.PhaseOffset
			+ ModeElapsed * VortexTurnsPerSecond * SpectacleDiceRoll::TwoPi;
		const float Radius = VortexRadius * (0.72f + 0.18f * FMath::Sin(ModeElapsed * 2.1f + State.PhaseOffset));
		const float HeightOffset = (static_cast<float>(Index) - (ActiveDice.Num() - 1) * 0.5f) * 5.2f
			+ FMath::Sin(ModeElapsed * 3.4f + State.PhaseOffset) * 4.0f;
		const FVector OrbitOffset = AxisX * FMath::Cos(Angle) * Radius
			+ AxisY * FMath::Sin(Angle) * Radius
			+ BoardUp * HeightOffset;
		const FVector LiftedTarget = EffectAnchor + OrbitOffset;
		const FVector Target = FMath::Lerp(Body->GetComponentLocation() + BoardUp * 4.0f, LiftedTarget, RiseAlpha);
		FVector Acceleration = (Target - Body->GetComponentLocation()) * 50.0f
			- Body->GetPhysicsLinearVelocity() * 8.0f;
		const FVector Tangent = (-AxisX * FMath::Sin(Angle) + AxisY * FMath::Cos(Angle));
		Acceleration += Tangent * (VortexRadius * VortexTurnsPerSecond * 5.2f);
		Body->AddForce(Acceleration.GetClampedToMaxSize(12500.0f), NAME_None, true);
		const FVector DesiredSpin = (State.SpinAxis + BoardUp * 0.35f).GetSafeNormal() * 4.5f;
		Body->AddTorqueInRadians(((DesiredSpin - Body->GetPhysicsAngularVelocityInRadians()) * 4.0f)
			.GetClampedToMaxSize(18.0f), NAME_None, true);
		State.bHasBeenAirborne |= GetBoardClearance(Body) >= MinimumAirborneClearance;
	}
	if (ModeElapsed >= VortexDuration)
	{
		ReleaseVortex();
	}
}

void ASpectacleDiceRollManager::ReleaseVortex()
{
	if (Mode == ESpectacleDiceMode::VortexPredicted)
	{
		ReleasePredictedVortex();
		return;
	}
	ReleaseNaturalBodies(-GetBoardUpVector() * 18.0f, 68.0f, 38.0f);
	PublishStatus(NSLOCTEXT("SpectacleDice", "VortexNaturalReleased",
		"Вихрь распался: кубики падают, результат всё ещё неизвестен…"));
}

void ASpectacleDiceRollManager::ReleasePredictedVortex()
{
	for (FActiveDie& State : ActiveDice)
	{
		UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(GuidedRoll) || !IsValid(Body))
		{
			continue;
		}

		FGuidedSettings& Saved = State.GuidedSettings;
		Saved.UpwardSpeed = GuidedRoll->UpwardSpeed;
		Saved.HorizontalSpeed = GuidedRoll->HorizontalSpeed;
		Saved.SpinSpeed = GuidedRoll->SpinSpeed;
		Saved.AirLinearDamping = GuidedRoll->AirLinearDamping;
		Saved.AirAngularDamping = GuidedRoll->AirAngularDamping;
		Saved.FreeFlightTime = GuidedRoll->FreeFlightTime;
		Saved.AerialAlignmentLeadTime = GuidedRoll->AerialAlignmentLeadTime;
		Saved.OrientationStrength = GuidedRoll->OrientationStrength;
		Saved.OrientationDamping = GuidedRoll->OrientationDamping;
		Saved.MaxAngularAcceleration = GuidedRoll->MaxAngularAcceleration;
		Saved.LandingLinearDamping = GuidedRoll->LandingLinearDamping;
		Saved.LandingAngularDamping = GuidedRoll->LandingAngularDamping;
		Saved.RequiredStableTime = GuidedRoll->RequiredStableTime;
		Saved.MaximumRollTime = GuidedRoll->MaximumRollTime;
		Saved.bAssistOnlyWhileFalling = GuidedRoll->bAssistOnlyWhileFalling;
		Saved.bUseAirborneSafetyAlignment = GuidedRoll->bUseAirborneSafetyAlignment;
		Saved.bRetryWrongFaceWithBounce = GuidedRoll->bRetryWrongFaceWithBounce;
		Saved.bFreezeAfterLanding = GuidedRoll->bFreezeAfterLanding;
		Saved.PhysicalMaterial = GuidedRoll->PhysicalMaterialOverride;

		GuidedRoll->UpwardSpeed = 22.0f;
		GuidedRoll->HorizontalSpeed = 48.0f;
		GuidedRoll->SpinSpeed = 25.0f;
		GuidedRoll->AirLinearDamping = 0.045f;
		GuidedRoll->AirAngularDamping = 0.04f;
		GuidedRoll->FreeFlightTime = 0.10f;
		GuidedRoll->AerialAlignmentLeadTime = 0.72f;
		GuidedRoll->OrientationStrength = 72.0f;
		GuidedRoll->OrientationDamping = 8.7f;
		GuidedRoll->MaxAngularAcceleration = 175.0f;
		GuidedRoll->LandingLinearDamping = 1.25f;
		GuidedRoll->LandingAngularDamping = 7.8f;
		GuidedRoll->RequiredStableTime = 0.22f;
		GuidedRoll->MaximumRollTime = 5.5f;
		GuidedRoll->bAssistOnlyWhileFalling = true;
		GuidedRoll->bUseAirborneSafetyAlignment = true;
		GuidedRoll->bRetryWrongFaceWithBounce = true;
		GuidedRoll->bFreezeAfterLanding = true;
		GuidedRoll->PhysicalMaterialOverride = SpectaclePhysicalMaterial;

		Body->SetPhysicsLinearVelocity(Body->GetPhysicsLinearVelocity().GetClampedToMaxSize(255.0f));
		Body->SetEnableGravity(true);
		State.Result = FMath::RandRange(1, 6);
		SetGeneratedNumber(State.Dice.Get(), State.Result);
		GuidedRoll->RollToRotation(State.Result, GetLandingRotation(State.Result));
	}
	Phase = ESpectacleDicePhase::PredictedSettling;
	PhaseElapsed = 0.0f;
	StableElapsed = 0.0f;
	PublishStatus(NSLOCTEXT("SpectacleDice", "VortexPredictedReleased",
		"Вихрь выпустил кубики: выбранные грани направляются физикой…"));
}

void ASpectacleDiceRollManager::UpdateMeteors(const float DeltaSeconds)
{
	bool bAllLaunched = true;
	for (int32 Index = 0; Index < ActiveDice.Num(); ++Index)
	{
		FActiveDie& State = ActiveDice[Index];
		if (State.bMeteorLaunched)
		{
			continue;
		}
		bAllLaunched = false;
		if (ModeElapsed < Index * MeteorInterval)
		{
			continue;
		}
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			State.bMeteorLaunched = true;
			continue;
		}
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Body->SetEnableGravity(true);
		Body->SetSimulatePhysics(true);
		Body->SetPhysicsLinearVelocity(-GetBoardUpVector()
			* FMath::FRandRange(165.0f + Index * 14.0f, 215.0f + Index * 18.0f));
		Body->SetPhysicsAngularVelocityInRadians(State.SpinAxis * FMath::FRandRange(20.0f, 38.0f));
		Body->WakeAllRigidBodies();
		State.bHasBeenAirborne = true;
		State.bMeteorLaunched = true;
	}
	if (!bAllLaunched)
	{
		bAllLaunched = ActiveDice.ContainsByPredicate([](const FActiveDie& State)
		{
			return !State.bMeteorLaunched;
		}) == false;
	}
	if (bAllLaunched)
	{
		Phase = ESpectacleDicePhase::NaturalSettling;
		PhaseElapsed = 0.0f;
		StableElapsed = 0.0f;
		PublishStatus(NSLOCTEXT("SpectacleDice", "MeteorsReleased",
			"Все метеоры на доске — ждём полной остановки…"));
	}
}

void ASpectacleDiceRollManager::UpdateGravityFlip(const float DeltaSeconds)
{
	const FVector BoardUp = GetBoardUpVector();
	for (int32 Index = 0; Index < ActiveDice.Num(); ++Index)
	{
		FActiveDie& State = ActiveDice[Index];
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		const FVector Target = EffectAnchor + GetCompactOffset(Index, ActiveDice.Num(), 17.0f)
			+ BoardUp * FMath::Sin(ModeElapsed * 3.0f + State.PhaseOffset) * 5.0f;
		const FVector Swirl = FVector::CrossProduct(BoardUp, Body->GetComponentLocation() - EffectAnchor)
			.GetClampedToMaxSize(75.0f);
		FVector Acceleration = (Target - Body->GetComponentLocation()) * 24.0f
			- Body->GetPhysicsLinearVelocity() * 5.5f + Swirl;
		Body->AddForce(Acceleration.GetClampedToMaxSize(7200.0f), NAME_None, true);
		Body->AddTorqueInRadians((State.SpinAxis * 3.6f - Body->GetPhysicsAngularVelocityInRadians())
			.GetClampedToMaxSize(13.0f) * 3.0f, NAME_None, true);
		State.bHasBeenAirborne |= GetBoardClearance(Body) >= MinimumAirborneClearance;
	}
	if (ModeElapsed >= GravityFlipDuration)
	{
		ReleaseNaturalBodies(-BoardUp * 72.0f, 58.0f, 36.0f);
		PublishStatus(NSLOCTEXT("SpectacleDice", "GravityReleased",
			"Гравитация вернулась — облако обрушилось на доску…"));
	}
}

void ASpectacleDiceRollManager::UpdateHandful(const float DeltaSeconds)
{
	FVector CursorAnchor;
	if (GetCursorBoardPoint(HandfulHeight, CursorAnchor))
	{
		EffectAnchor = CursorAnchor;
	}
	const FVector BoardUp = GetBoardUpVector();
	for (int32 Index = 0; Index < ActiveDice.Num(); ++Index)
	{
		FActiveDie& State = ActiveDice[Index];
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		const FVector Target = EffectAnchor + GetCompactOffset(Index, ActiveDice.Num(), HandfulSpacing)
			+ BoardUp * FMath::Sin(ModeElapsed * 2.2f + State.PhaseOffset) * 1.2f;
		FVector Acceleration = (Target - Body->GetComponentLocation()) * 58.0f
			- Body->GetPhysicsLinearVelocity() * 11.0f;
		Body->AddForce(Acceleration.GetClampedToMaxSize(9500.0f), NAME_None, true);
		Body->AddTorqueInRadians((State.SpinAxis * 1.5f - Body->GetPhysicsAngularVelocityInRadians())
			.GetClampedToMaxSize(10.0f) * 3.0f, NAME_None, true);
		State.bHasBeenAirborne |= GetBoardClearance(Body) >= MinimumAirborneClearance;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController))
	{
		return;
	}
	const bool bLeftDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (bWaitForActivationClickRelease)
	{
		if (!bLeftDown)
		{
			bWaitForActivationClickRelease = false;
			PreviousHandfulAnchor = EffectAnchor;
		}
	}
	else if (bLeftDown && !bPreviousLeftMouseDown)
	{
		bGestureDragging = true;
		FilteredGestureVelocity = FVector::ZeroVector;
		PreviousHandfulAnchor = EffectAnchor;
		PublishStatus(NSLOCTEXT("SpectacleDice", "HandfulDragging",
			"Пригоршня следует за рукой — отпустите ЛКМ для броска."));
	}
	else if (bLeftDown && bGestureDragging)
	{
		const FVector RawVelocity = (EffectAnchor - PreviousHandfulAnchor) / FMath::Max(DeltaSeconds, 0.001f);
		FilteredGestureVelocity = FMath::Lerp(FilteredGestureVelocity, RawVelocity, 0.32f);
		PreviousHandfulAnchor = EffectAnchor;
	}
	else if (!bLeftDown && bPreviousLeftMouseDown && bGestureDragging)
	{
		ReleaseHandful();
	}
	bPreviousLeftMouseDown = bLeftDown;
}

bool ASpectacleDiceRollManager::ReleaseHandful()
{
	if (Phase != ESpectacleDicePhase::HandfulAiming || ActiveDice.IsEmpty())
	{
		return false;
	}
	const FVector BoardUp = GetBoardUpVector();
	FVector TangentialVelocity = FilteredGestureVelocity
		- BoardUp * FVector::DotProduct(FilteredGestureVelocity, BoardUp);
	TangentialVelocity = (TangentialVelocity * HandfulGestureScale).GetClampedToMaxSize(520.0f);
	if (TangentialVelocity.SizeSquared() < FMath::Square(45.0f))
	{
		const FTransform BoardTransform = IsValid(BoardSurface)
			? BoardSurface->GetComponentTransform()
			: FTransform::Identity;
		TangentialVelocity = BoardTransform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal() * 105.0f;
	}
	ReleaseNaturalBodies(TangentialVelocity + BoardUp * 175.0f, 46.0f, 40.0f);
	PublishStatus(NSLOCTEXT("SpectacleDice", "HandfulReleased",
		"Пригоршня брошена: кубики разделяются в воздухе…"));
	return true;
}

void ASpectacleDiceRollManager::UpdateBackboardAiming(const float)
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController))
	{
		return;
	}

	const bool bLeftDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (bWaitForActivationClickRelease)
	{
		if (!bLeftDown)
		{
			bWaitForActivationClickRelease = false;
		}
	}
	else if (bLeftDown && !bPreviousLeftMouseDown)
	{
		FVector Target = GetBoardPoint(0.0f, 0.0f, 0.0f);
		GetCursorBoardPoint(0.0f, Target);
		ReleaseDirectedBackboardAt(Target);
	}
	bPreviousLeftMouseDown = bLeftDown;
}

bool ASpectacleDiceRollManager::PrepareBackboardStaging()
{
	if (!ResolveBackboardFrame())
	{
		return false;
	}

	for (int32 Index = 0; Index < ActiveDice.Num(); ++Index)
	{
		UPrimitiveComponent* Body = ActiveDice[Index].Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Body->SetSimulatePhysics(false);
		Body->SetEnableGravity(false);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetWorldLocation(GetBackboardSpawnPoint(Index), false, nullptr, ETeleportType::TeleportPhysics);
		Body->SetWorldRotation(FMath::VRand().Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	return true;
}

bool ASpectacleDiceRollManager::LaunchBackboardNatural(const FVector& Target)
{
	if (ActiveDice.IsEmpty())
	{
		return false;
	}

	OpenBackboardGate();
	const FVector BoardUp = GetBoardUpVector();
	int32 LaunchedCount = 0;
	for (FActiveDie& State : ActiveDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}

		FVector Direction = Target - Body->GetComponentLocation();
		Direction -= BoardUp * FVector::DotProduct(Direction, BoardUp);
		Direction = Direction.GetSafeNormal(SMALL_NUMBER, -BackboardOutwardDirection);
		const float HorizontalSpeed = FMath::FRandRange(
			FMath::Min(BackboardHorizontalSpeedMin, BackboardHorizontalSpeedMax),
			FMath::Max(BackboardHorizontalSpeedMin, BackboardHorizontalSpeedMax));
		const float UpwardSpeed = FMath::FRandRange(
			FMath::Min(BackboardUpwardSpeedMin, BackboardUpwardSpeedMax),
			FMath::Max(BackboardUpwardSpeedMin, BackboardUpwardSpeedMax));

		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Body->SetEnableGravity(true);
		Body->SetLinearDamping(0.04f);
		Body->SetAngularDamping(0.035f);
		Body->SetSimulatePhysics(true);
		Body->SetPhysicsLinearVelocity(Direction * HorizontalSpeed + BoardUp * UpwardSpeed);
		Body->SetPhysicsAngularVelocityInRadians(
			(State.SpinAxis + FMath::VRand() * 0.75f).GetSafeNormal()
			* FMath::FRandRange(30.0f, 43.0f));
		Body->WakeAllRigidBodies();
		State.bHasBeenAirborne = true;
		++LaunchedCount;
	}

	Phase = ESpectacleDicePhase::NaturalSettling;
	PhaseElapsed = 0.0f;
	StableElapsed = 0.0f;
	PublishStatus(NSLOCTEXT("SpectacleDice", "BackboardNaturalReleased",
		"Кубики влетели из-за доски — результат определит только Chaos…"));
	return LaunchedCount > 0;
}

bool ASpectacleDiceRollManager::LaunchBackboardPredicted(const FVector& Target)
{
	if (ActiveDice.IsEmpty())
	{
		return false;
	}

	const FVector BoardUp = GetBoardUpVector();
	int32 LaunchedCount = 0;
	for (FActiveDie& State : ActiveDice)
	{
		UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(GuidedRoll) || !IsValid(Body))
		{
			continue;
		}

		FGuidedSettings& Saved = State.GuidedSettings;
		Saved.UpwardSpeed = GuidedRoll->UpwardSpeed;
		Saved.HorizontalSpeed = GuidedRoll->HorizontalSpeed;
		Saved.SpinSpeed = GuidedRoll->SpinSpeed;
		Saved.AirLinearDamping = GuidedRoll->AirLinearDamping;
		Saved.AirAngularDamping = GuidedRoll->AirAngularDamping;
		Saved.FreeFlightTime = GuidedRoll->FreeFlightTime;
		Saved.AerialAlignmentLeadTime = GuidedRoll->AerialAlignmentLeadTime;
		Saved.OrientationStrength = GuidedRoll->OrientationStrength;
		Saved.OrientationDamping = GuidedRoll->OrientationDamping;
		Saved.MaxAngularAcceleration = GuidedRoll->MaxAngularAcceleration;
		Saved.LandingLinearDamping = GuidedRoll->LandingLinearDamping;
		Saved.LandingAngularDamping = GuidedRoll->LandingAngularDamping;
		Saved.RequiredStableTime = GuidedRoll->RequiredStableTime;
		Saved.MaximumRollTime = GuidedRoll->MaximumRollTime;
		Saved.bAssistOnlyWhileFalling = GuidedRoll->bAssistOnlyWhileFalling;
		Saved.bUseAirborneSafetyAlignment = GuidedRoll->bUseAirborneSafetyAlignment;
		Saved.bRetryWrongFaceWithBounce = GuidedRoll->bRetryWrongFaceWithBounce;
		Saved.bFreezeAfterLanding = GuidedRoll->bFreezeAfterLanding;
		Saved.PhysicalMaterial = GuidedRoll->PhysicalMaterialOverride;

		GuidedRoll->UpwardSpeed = 0.0f;
		GuidedRoll->HorizontalSpeed = 0.0f;
		GuidedRoll->SpinSpeed = 0.0f;
		GuidedRoll->AirLinearDamping = 0.045f;
		GuidedRoll->AirAngularDamping = 0.04f;
		GuidedRoll->FreeFlightTime = 0.16f;
		GuidedRoll->AerialAlignmentLeadTime = 0.76f;
		GuidedRoll->OrientationStrength = 72.0f;
		GuidedRoll->OrientationDamping = 8.7f;
		GuidedRoll->MaxAngularAcceleration = 175.0f;
		GuidedRoll->LandingLinearDamping = 1.25f;
		GuidedRoll->LandingAngularDamping = 7.8f;
		GuidedRoll->RequiredStableTime = 0.22f;
		GuidedRoll->MaximumRollTime = 6.0f;
		GuidedRoll->bAssistOnlyWhileFalling = true;
		GuidedRoll->bUseAirborneSafetyAlignment = true;
		GuidedRoll->bRetryWrongFaceWithBounce = true;
		GuidedRoll->bFreezeAfterLanding = true;
		GuidedRoll->PhysicalMaterialOverride = SpectaclePhysicalMaterial;

		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		State.Result = FMath::RandRange(1, 6);
		SetGeneratedNumber(State.Dice.Get(), State.Result);
		if (!GuidedRoll->RollToRotation(State.Result, GetLandingRotation(State.Result)))
		{
			RestoreGuidedSettings(State);
			State.Result = 0;
			SetGeneratedNumber(State.Dice.Get(), 0);
			continue;
		}

		FVector Direction = Target - Body->GetComponentLocation();
		Direction -= BoardUp * FVector::DotProduct(Direction, BoardUp);
		Direction = Direction.GetSafeNormal(SMALL_NUMBER, -BackboardOutwardDirection);
		const float HorizontalSpeed = FMath::FRandRange(
			FMath::Min(BackboardHorizontalSpeedMin, BackboardHorizontalSpeedMax),
			FMath::Max(BackboardHorizontalSpeedMin, BackboardHorizontalSpeedMax));
		const float UpwardSpeed = FMath::FRandRange(
			FMath::Min(BackboardUpwardSpeedMin, BackboardUpwardSpeedMax),
			FMath::Max(BackboardUpwardSpeedMin, BackboardUpwardSpeedMax));
		Body->SetPhysicsLinearVelocity(Direction * HorizontalSpeed + BoardUp * UpwardSpeed);
		Body->SetPhysicsAngularVelocityInRadians(
			(State.SpinAxis + FMath::VRand() * 0.65f).GetSafeNormal()
			* FMath::FRandRange(25.0f, 36.0f));
		Body->WakeAllRigidBodies();
		++LaunchedCount;
	}

	// RollToRotation refreshes the shared boundary colliders. Reapply the visible-rim inset,
	// then open only the camera-far wall long enough for this incoming throw.
	SetBoardBoundaryInset(BoardWallInset, ActiveDice[0].Body.Get());
	OpenBackboardGate();
	Phase = ESpectacleDicePhase::PredictedSettling;
	PhaseElapsed = 0.0f;
	StableElapsed = 0.0f;
	PublishStatus(NSLOCTEXT("SpectacleDice", "BackboardPredictedReleased",
		"Бросок из-за доски начался — выбранные грани направляются в полёте…"));
	return LaunchedCount > 0;
}

bool ASpectacleDiceRollManager::ReleaseDirectedBackboardAt(const FVector& Target)
{
	if (Mode != ESpectacleDiceMode::BackboardDirected
		|| Phase != ESpectacleDicePhase::BackboardAiming)
	{
		return false;
	}
	const bool bReleased = LaunchBackboardNatural(Target);
	if (bReleased)
	{
		PublishStatus(NSLOCTEXT("SpectacleDice", "BackboardDirectedReleased",
			"Направленный бросок ушёл к выбранной точке — результат ещё неизвестен…"));
	}
	return bReleased;
}

bool ASpectacleDiceRollManager::ResolveBackboardFrame()
{
	if (!IsValid(ResolveBoardSurface()))
	{
		return false;
	}

	const FTransform Transform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = Transform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector LocalTop(Bounds.Origin.X, Bounds.Origin.Y,
		Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z);
	const FVector BoardCentre = Transform.TransformPosition(LocalTop);
	const FVector BoardUp = GetBoardUpVector();

	FVector TowardFarSide = Transform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager))
	{
		TowardFarSide = BoardCentre - PlayerController->PlayerCameraManager->GetCameraLocation();
		TowardFarSide -= BoardUp * FVector::DotProduct(TowardFarSide, BoardUp);
		TowardFarSide = TowardFarSide.GetSafeNormal(SMALL_NUMBER,
			Transform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal());
	}

	const FVector LocalDirection = Transform.InverseTransformVectorNoScale(TowardFarSide);
	FVector LocalEdge = LocalTop;
	if (FMath::Abs(LocalDirection.X) >= FMath::Abs(LocalDirection.Y))
	{
		BackboardGateAxis = 0;
		BackboardGateSign = LocalDirection.X >= 0.0f ? 1.0f : -1.0f;
		LocalEdge.X += BackboardGateSign * Bounds.BoxExtent.X;
		BackboardOutwardDirection = Transform.TransformVectorNoScale(
			FVector(BackboardGateSign, 0.0f, 0.0f)).GetSafeNormal();
		BackboardSideDirection = Transform.TransformVectorNoScale(FVector::RightVector).GetSafeNormal();
		BackboardGateName = BackboardGateSign > 0.0f
			? TEXT("DiceBoundary_XMax") : TEXT("DiceBoundary_XMin");
	}
	else
	{
		BackboardGateAxis = 1;
		BackboardGateSign = LocalDirection.Y >= 0.0f ? 1.0f : -1.0f;
		LocalEdge.Y += BackboardGateSign * Bounds.BoxExtent.Y;
		BackboardOutwardDirection = Transform.TransformVectorNoScale(
			FVector(0.0f, BackboardGateSign, 0.0f)).GetSafeNormal();
		BackboardSideDirection = Transform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
		BackboardGateName = BackboardGateSign > 0.0f
			? TEXT("DiceBoundary_YMax") : TEXT("DiceBoundary_YMin");
	}

	BackboardSpawnCenter = Transform.TransformPosition(LocalEdge)
		+ BackboardOutwardDirection * BackboardOutsideDistance
		+ BoardUp * BackboardLaunchHeight;
	return !BackboardSpawnCenter.ContainsNaN() && BackboardGateName != NAME_None;
}

FVector ASpectacleDiceRollManager::GetBackboardSpawnPoint(const int32 Index) const
{
	const int32 Column = Index % 3 - 1;
	const int32 Row = Index / 3;
	return BackboardSpawnCenter
		+ BackboardSideDirection * (Column * BackboardDiceSpacing)
		+ BackboardOutwardDirection * (Row * BackboardDiceSpacing * 0.86f)
		+ GetBoardUpVector() * (Row * 3.0f + FMath::Abs(Column) * 1.2f);
}

FVector ASpectacleDiceRollManager::GetAutomaticBackboardTarget() const
{
	return GetBoardPoint(FMath::FRandRange(-0.20f, 0.20f), FMath::FRandRange(-0.20f, 0.20f), 0.0f);
}

void ASpectacleDiceRollManager::OpenBackboardGate()
{
	if (!IsValid(BoardActor) || BackboardGateName == NAME_None)
	{
		return;
	}
	if (UBoxComponent* Gate = FindObject<UBoxComponent>(BoardActor, *BackboardGateName.ToString()))
	{
		Gate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bBackboardGateOpen = true;
		BackboardGateElapsed = 0.0f;
	}
}

void ASpectacleDiceRollManager::UpdateBackboardGate(const float DeltaSeconds)
{
	if (!bBackboardGateOpen || !IsValid(BoardSurface) || BackboardGateAxis == INDEX_NONE)
	{
		return;
	}

	BackboardGateElapsed += DeltaSeconds;
	const FTransform Transform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const float EdgeCoordinate = BackboardGateAxis == 0
		? Bounds.Origin.X + BackboardGateSign * Bounds.BoxExtent.X
		: Bounds.Origin.Y + BackboardGateSign * Bounds.BoxExtent.Y;
	bool bAllInside = BackboardGateElapsed >= 0.16f;
	for (const FActiveDie& State : ActiveDice)
	{
		const UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		const FVector Local = Transform.InverseTransformPosition(Body->GetComponentLocation());
		const float Coordinate = BackboardGateAxis == 0 ? Local.X : Local.Y;
		bAllInside = bAllInside && (Coordinate - EdgeCoordinate) * BackboardGateSign <= -0.5f;
	}
	if (bAllInside)
	{
		CloseBackboardGate();
		return;
	}

	if (BackboardGateElapsed < 1.25f)
	{
		return;
	}

	// A very slow or deflected die must not remain outside forever. Move only such outliers just
	// inside the opened rim and preserve a small inward/upward velocity before closing the gate.
	const FVector AbsScale = Transform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector BoardUp = GetBoardUpVector();
	for (FActiveDie& State : ActiveDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		FVector Local = Transform.InverseTransformPosition(Body->GetComponentLocation());
		double& Coordinate = BackboardGateAxis == 0 ? Local.X : Local.Y;
		if ((Coordinate - EdgeCoordinate) * BackboardGateSign <= 0.0f)
		{
			continue;
		}
		const float AxisScale = BackboardGateAxis == 0 ? AbsScale.X : AbsScale.Y;
		Coordinate = EdgeCoordinate - BackboardGateSign * (BoardWallInset + 14.0f) / AxisScale;
		Local.Z = FMath::Max(Local.Z, Bounds.Origin.Z + Bounds.BoxExtent.Z
			+ 10.0f / AbsScale.Z);
		Body->SetWorldLocation(Transform.TransformPosition(Local), false, nullptr, ETeleportType::TeleportPhysics);
		Body->SetPhysicsLinearVelocity(-BackboardOutwardDirection * 115.0f + BoardUp * 85.0f);
		Body->WakeAllRigidBodies();
		State.bHasBeenAirborne = true;
	}
	CloseBackboardGate();
}

void ASpectacleDiceRollManager::CloseBackboardGate()
{
	if (IsValid(BoardActor) && BackboardGateName != NAME_None)
	{
		if (UBoxComponent* Gate = FindObject<UBoxComponent>(BoardActor, *BackboardGateName.ToString()))
		{
			Gate->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
	bBackboardGateOpen = false;
	BackboardGateElapsed = 0.0f;
}

void ASpectacleDiceRollManager::ReleaseNaturalBodies(const FVector& SharedVelocity,
	const float RandomHorizontalSpeed, const float SpinSpeed)
{
	const FVector BoardUp = GetBoardUpVector();
	for (FActiveDie& State : ActiveDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			continue;
		}
		FVector RandomDirection = FMath::VRand();
		RandomDirection -= BoardUp * FVector::DotProduct(RandomDirection, BoardUp);
		RandomDirection = RandomDirection.GetSafeNormal();
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Body->SetEnableGravity(true);
		Body->SetLinearDamping(0.04f);
		Body->SetAngularDamping(0.035f);
		Body->AddImpulse(SharedVelocity
			+ RandomDirection * FMath::FRandRange(RandomHorizontalSpeed * 0.35f, RandomHorizontalSpeed),
			NAME_None, true);
		Body->AddAngularImpulseInRadians((State.SpinAxis + FMath::VRand() * 0.65f).GetSafeNormal()
			* FMath::FRandRange(SpinSpeed * 0.72f, SpinSpeed), NAME_None, true);
		Body->WakeAllRigidBodies();
	}
	Phase = ESpectacleDicePhase::NaturalSettling;
	PhaseElapsed = 0.0f;
	StableElapsed = 0.0f;
}

void ASpectacleDiceRollManager::UpdateNaturalSettling(const float DeltaSeconds)
{
	PhaseElapsed += DeltaSeconds;
	bool bAllSettled = PhaseElapsed >= 0.72f;
	for (FActiveDie& State : ActiveDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			bAllSettled = false;
			continue;
		}
		State.bHasBeenAirborne |= GetBoardClearance(Body) >= MinimumAirborneClearance;
		RecoverEscapedDie(State);
		if (PhaseElapsed >= 2.35f)
		{
			const float Alpha = FMath::Clamp((PhaseElapsed - 2.35f) / 1.45f, 0.0f, 1.0f);
			Body->SetLinearDamping(FMath::Lerp(0.04f, 1.42f, Alpha));
			Body->SetAngularDamping(FMath::Lerp(0.035f, 4.8f, Alpha));
		}
		bAllSettled = bAllSettled && State.bHadSupportContact
			&& Body->GetPhysicsLinearVelocity().Size() <= 10.0f
			&& Body->GetPhysicsAngularVelocityInRadians().Size() <= 0.58f;
	}
	StableElapsed = bAllSettled ? StableElapsed + DeltaSeconds : 0.0f;
	if (StableElapsed >= 0.30f || PhaseElapsed >= 8.0f)
	{
		FinishNaturalRoll();
	}
}

void ASpectacleDiceRollManager::UpdatePredictedSettling(const float DeltaSeconds)
{
	PhaseElapsed += DeltaSeconds;
	const bool bAnyRolling = ActiveDice.ContainsByPredicate([](const FActiveDie& State)
	{
		const UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
		return IsValid(GuidedRoll) && GuidedRoll->IsRolling();
	});
	if ((!bAnyRolling && PhaseElapsed >= 0.25f) || PhaseElapsed >= 7.0f)
	{
		FinishPredictedRoll();
	}
}

void ASpectacleDiceRollManager::HandleNaturalHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!IsActive()
		|| Mode == ESpectacleDiceMode::VortexPredicted
		|| Mode == ESpectacleDiceMode::BackboardPredicted
		|| !IsValid(HitComponent))
	{
		return;
	}
	FActiveDie* State = ActiveDice.FindByPredicate([HitComponent](const FActiveDie& Candidate)
	{
		return Candidate.Body.Get() == HitComponent;
	});
	if (!State)
	{
		return;
	}

	const FVector BoardUp = GetBoardUpVector();
	const bool bHitBoard = OtherActor == BoardActor || OtherComponent == BoardSurface
		|| (IsValid(OtherComponent) && OtherComponent->ComponentHasTag(TEXT("DiceBoundaryWall")));
	const bool bSupport = FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), BoardUp) >= 0.50f;
	const bool bLaunchContact = bHitBoard && bSupport && !State->bHasBeenAirborne;
	if (bHitBoard && !bLaunchContact && DiceImpactFeedbackBridge::PlayMatchingGuidedImpact(
		State->GuidedRoll.Get(), HitComponent, NormalImpulse, Hit, State->LastImpactFeedbackTime))
	{
		++LastImpactFeedbackCount;
	}
	if (State->bHasBeenAirborne && bSupport)
	{
		State->bHadSupportContact = true;
	}

	const bool bHitDice = IsValid(OtherComponent) && OtherComponent->IsSimulatingPhysics()
		&& ActiveDice.ContainsByPredicate([OtherComponent](const FActiveDie& Candidate)
		{
			return Candidate.Body.Get() == OtherComponent;
		});
	if (!bHitDice || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (State->LastCollisionKickTime >= 0.0 && Now - State->LastCollisionKickTime < 0.075)
	{
		return;
	}
	State->LastCollisionKickTime = Now;
	FVector KickAxis = (FVector::CrossProduct(Hit.ImpactNormal, BoardUp) + FMath::VRand() * 0.8f).GetSafeNormal();
	HitComponent->AddAngularImpulseInRadians(KickAxis * 3.8f, NAME_None, true);
}

void ASpectacleDiceRollManager::FinishNaturalRoll()
{
	CloseBackboardGate();
	TArray<int32> Results;
	Results.Reserve(ActiveDice.Num());
	LastFinishedDice.Reset();
	LastFinishedDice.Reserve(ActiveDice.Num());
	UPrimitiveComponent* BoundaryBody = nullptr;
	for (FActiveDie& State : ActiveDice)
	{
		BoundaryBody = IsValid(BoundaryBody) ? BoundaryBody : State.Body.Get();
		const int32 Result = IsValid(State.Body.Get())
			? ANaturalDiceRollManager::DetermineTopFace(State.Body->GetComponentQuat(), GetBoardUpVector())
			: 0;
		State.Result = Result;
		Results.Add(Result);
		LastFinishedDice.Add(State.Dice.Get());
		SetGeneratedNumber(State.Dice.Get(), Result);
		RestoreDie(State);
		if (UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get())
		{
			GuidedRoll->OnDiceRollFinished.Broadcast(Result);
		}
	}
	SetBoardBoundaryInset(0.0f, BoundaryBody);

	FString Values;
	for (const int32 Result : Results)
	{
		Values += Values.IsEmpty() ? FString::FromInt(Result) : TEXT(" · ") + FString::FromInt(Result);
	}
	ActiveDice.Reset();
	Mode = ESpectacleDiceMode::None;
	Phase = ESpectacleDicePhase::Idle;
	SetActorTickEnabled(false);
	PublishStatus(FText::Format(NSLOCTEXT("SpectacleDice", "NaturalFinished",
		"Физический результат: {0}"), FText::FromString(Values)));
	OnFinished.Broadcast(Results);
}

void ASpectacleDiceRollManager::FinishPredictedRoll()
{
	const bool bWasBackboardThrow = Mode == ESpectacleDiceMode::BackboardPredicted;
	CloseBackboardGate();
	TArray<int32> Results;
	Results.Reserve(ActiveDice.Num());
	LastFinishedDice.Reset();
	LastFinishedDice.Reserve(ActiveDice.Num());
	UPrimitiveComponent* BoundaryBody = nullptr;
	for (FActiveDie& State : ActiveDice)
	{
		BoundaryBody = IsValid(BoundaryBody) ? BoundaryBody : State.Body.Get();
		Results.Add(State.Result);
		LastFinishedDice.Add(State.Dice.Get());
		if (UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
			IsValid(GuidedRoll) && GuidedRoll->IsRolling())
		{
			GuidedRoll->CancelRoll(true);
		}
		RestoreGuidedSettings(State);
		RestoreDie(State);
	}
	SetBoardBoundaryInset(0.0f, BoundaryBody);

	FString Values;
	for (const int32 Result : Results)
	{
		Values += Values.IsEmpty() ? FString::FromInt(Result) : TEXT(" · ") + FString::FromInt(Result);
	}
	ActiveDice.Reset();
	Mode = ESpectacleDiceMode::None;
	Phase = ESpectacleDicePhase::Idle;
	SetActorTickEnabled(false);
	PublishStatus(FText::Format(bWasBackboardThrow
		? NSLOCTEXT("SpectacleDice", "BackboardPredictedFinished", "Из-за доски с прогнозом: {0}")
		: NSLOCTEXT("SpectacleDice", "PredictedFinished", "Вихрь с прогнозом: {0}"),
		FText::FromString(Values)));
	OnFinished.Broadcast(Results);
}

TArray<ACPP_Dice*> ASpectacleDiceRollManager::GetLastFinishedDice() const
{
	TArray<ACPP_Dice*> Result;
	Result.Reserve(LastFinishedDice.Num());
	for (ACPP_Dice* Dice : LastFinishedDice)
	{
		Result.Add(Dice);
	}
	return Result;
}

void ASpectacleDiceRollManager::CancelSpecialRoll()
{
	if (ActiveDice.IsEmpty())
	{
		CloseBackboardGate();
		Mode = ESpectacleDiceMode::None;
		Phase = ESpectacleDicePhase::Idle;
		SetActorTickEnabled(false);
		return;
	}
	CloseBackboardGate();
	UPrimitiveComponent* BoundaryBody = nullptr;
	for (FActiveDie& State : ActiveDice)
	{
		BoundaryBody = IsValid(BoundaryBody) ? BoundaryBody : State.Body.Get();
		if (UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
			IsValid(GuidedRoll) && GuidedRoll->IsRolling())
		{
			GuidedRoll->CancelRoll(true);
		}
		RestoreGuidedSettings(State);
		RestoreDie(State);
	}
	SetBoardBoundaryInset(0.0f, BoundaryBody);
	ActiveDice.Reset();
	Mode = ESpectacleDiceMode::None;
	Phase = ESpectacleDicePhase::Idle;
	SetActorTickEnabled(false);
}

void ASpectacleDiceRollManager::RestoreGuidedSettings(FActiveDie& State)
{
	UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
	if (!IsValid(GuidedRoll) || State.Result == 0)
	{
		return;
	}
	const FGuidedSettings& Saved = State.GuidedSettings;
	GuidedRoll->UpwardSpeed = Saved.UpwardSpeed;
	GuidedRoll->HorizontalSpeed = Saved.HorizontalSpeed;
	GuidedRoll->SpinSpeed = Saved.SpinSpeed;
	GuidedRoll->AirLinearDamping = Saved.AirLinearDamping;
	GuidedRoll->AirAngularDamping = Saved.AirAngularDamping;
	GuidedRoll->FreeFlightTime = Saved.FreeFlightTime;
	GuidedRoll->AerialAlignmentLeadTime = Saved.AerialAlignmentLeadTime;
	GuidedRoll->OrientationStrength = Saved.OrientationStrength;
	GuidedRoll->OrientationDamping = Saved.OrientationDamping;
	GuidedRoll->MaxAngularAcceleration = Saved.MaxAngularAcceleration;
	GuidedRoll->LandingLinearDamping = Saved.LandingLinearDamping;
	GuidedRoll->LandingAngularDamping = Saved.LandingAngularDamping;
	GuidedRoll->RequiredStableTime = Saved.RequiredStableTime;
	GuidedRoll->MaximumRollTime = Saved.MaximumRollTime;
	GuidedRoll->bAssistOnlyWhileFalling = Saved.bAssistOnlyWhileFalling;
	GuidedRoll->bUseAirborneSafetyAlignment = Saved.bUseAirborneSafetyAlignment;
	GuidedRoll->bRetryWrongFaceWithBounce = Saved.bRetryWrongFaceWithBounce;
	GuidedRoll->bFreezeAfterLanding = Saved.bFreezeAfterLanding;
	GuidedRoll->PhysicalMaterialOverride = Saved.PhysicalMaterial.Get();
}

void ASpectacleDiceRollManager::RestoreDie(FActiveDie& State)
{
	UPrimitiveComponent* Body = State.Body.Get();
	if (IsValid(Body))
	{
		Body->OnComponentHit.RemoveDynamic(this, &ASpectacleDiceRollManager::HandleNaturalHit);
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		// The map keeps its idle dice query-only and non-simulating. Stop Chaos before restoring that
		// collision mode, otherwise BodyInstance warns about a transient simulate/query-only combination.
		Body->SetSimulatePhysics(false);
		Body->SetLinearDamping(State.OriginalLinearDamping);
		Body->SetAngularDamping(State.OriginalAngularDamping);
		Body->SetNotifyRigidBodyCollision(State.bOriginalNotifyRigidBodyCollision);
		Body->SetUseCCD(State.bOriginalUseCCD);
		Body->SetCollisionEnabled(State.OriginalCollisionEnabled);
		Body->SetCollisionResponseToChannel(ECC_WorldDynamic, State.OriginalWorldDynamicResponse);
		Body->SetPhysMaterialOverride(State.OriginalPhysicalMaterial.Get());
		Body->SetEnableGravity(State.bOriginalGravity);
		if (State.bOriginalSimulatePhysics)
		{
			Body->SetSimulatePhysics(true);
		}
		if (!State.bOriginalSimulatePhysics && State.OriginalAttachParent.IsValid())
		{
			Body->AttachToComponent(State.OriginalAttachParent.Get(),
				FAttachmentTransformRules::KeepWorldTransform, State.OriginalAttachSocket);
		}
	}
	if (ACPP_Dice* Dice = State.Dice.Get())
	{
		Dice->SetCanRollDice(State.bOriginalCanRoll);
	}
}

void ASpectacleDiceRollManager::PublishStatus(const FText& Status)
{
	OnStatusChanged.Broadcast(Status);
}

void ASpectacleDiceRollManager::SetGeneratedNumber(AActor* DiceActor, const int32 Result) const
{
	if (IsValid(DiceActor))
	{
		if (FIntProperty* Property = FindFProperty<FIntProperty>(DiceActor->GetClass(), TEXT("GeneratedNumber")))
		{
			Property->SetPropertyValue_InContainer(DiceActor, Result);
		}
	}
}

FRotator ASpectacleDiceRollManager::GetLandingRotation(const int32 Result) const
{
	switch (Result)
	{
	case 1: return FRotator(0.0f, 0.0f, 0.0f);
	case 2: return FRotator(0.0f, 0.0f, 90.0f);
	case 3: return FRotator(-90.0f, 0.0f, 0.0f);
	case 4: return FRotator(90.0f, 0.0f, 0.0f);
	case 5: return FRotator(0.0f, 0.0f, -90.0f);
	case 6: return FRotator(0.0f, 90.0f, 180.0f);
	default: return FRotator::ZeroRotator;
	}
}

FVector ASpectacleDiceRollManager::GetCompactOffset(const int32 Index, const int32 Count,
	const float Spacing) const
{
	if (Index < 0 || Count <= 1 || !IsValid(BoardSurface))
	{
		return FVector::ZeroVector;
	}
	static const FVector Slots[] =
	{
		FVector( 0.9397f,  0.0000f,  0.3420f),
		FVector(-0.9397f,  0.0000f, -0.3420f),
		FVector(-0.1170f,  0.9400f,  0.3210f),
		FVector( 0.1170f, -0.9400f, -0.3210f),
		FVector(-0.3215f, -0.3416f,  0.8832f),
		FVector( 0.3215f,  0.3416f, -0.8832f),
	};
	const int32 ActiveCount = FMath::Clamp(Count, 1, static_cast<int32>(UE_ARRAY_COUNT(Slots)));
	FVector Centroid = FVector::ZeroVector;
	for (int32 SlotIndex = 0; SlotIndex < ActiveCount; ++SlotIndex)
	{
		Centroid += Slots[SlotIndex];
	}
	Centroid /= ActiveCount;
	return BoardSurface->GetComponentTransform().TransformVectorNoScale(
		(Slots[Index % UE_ARRAY_COUNT(Slots)] - Centroid) * Spacing * 0.78f);
}

FVector ASpectacleDiceRollManager::GetBoardPoint(const float NormalizedX, const float NormalizedY,
	const float Height) const
{
	if (!IsValid(BoardSurface))
	{
		return GetActorLocation() + FVector(0.0f, 0.0f, Height);
	}
	const FTransform Transform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = Transform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector LocalPoint(
		Bounds.Origin.X + Bounds.BoxExtent.X * FMath::Clamp(NormalizedX, -0.72f, 0.72f),
		Bounds.Origin.Y + Bounds.BoxExtent.Y * FMath::Clamp(NormalizedY, -0.72f, 0.72f),
		Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z);
	return Transform.TransformPosition(LocalPoint) + GetBoardUpVector() * Height;
}

bool ASpectacleDiceRollManager::GetCursorBoardPoint(const float Height, FVector& OutPoint) const
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(BoardSurface))
	{
		return false;
	}
	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}
	const FVector PlanePoint = GetBoardPoint(0.0f, 0.0f, Height);
	FVector WorldPoint = FMath::LinePlaneIntersection(RayOrigin, RayOrigin + RayDirection * 100000.0f,
		FPlane(PlanePoint, GetBoardUpVector()));
	const FTransform Transform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = Transform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	FVector LocalPoint = Transform.InverseTransformPosition(WorldPoint);
	const float MarginX = (BoardWallInset + 12.0f) / AbsScale.X;
	const float MarginY = (BoardWallInset + 12.0f) / AbsScale.Y;
	LocalPoint.X = FMath::Clamp(LocalPoint.X, Bounds.Origin.X - Bounds.BoxExtent.X + MarginX,
		Bounds.Origin.X + Bounds.BoxExtent.X - MarginX);
	LocalPoint.Y = FMath::Clamp(LocalPoint.Y, Bounds.Origin.Y - Bounds.BoxExtent.Y + MarginY,
		Bounds.Origin.Y + Bounds.BoxExtent.Y - MarginY);
	OutPoint = Transform.TransformPosition(LocalPoint);
	return !OutPoint.ContainsNaN();
}

AActor* ASpectacleDiceRollManager::ResolveBoardActor()
{
	if (IsValid(BoardActor) || !GetWorld())
	{
		return BoardActor;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (It->ActorHasTag(TEXT("DiceBoard"))
			|| It->GetClass()->GetName().Contains(TEXT("BP_Board"))))
		{
			BoardActor = *It;
			break;
		}
	}
	return BoardActor;
}

UPrimitiveComponent* ASpectacleDiceRollManager::ResolveBoardSurface()
{
	if (IsValid(BoardSurface))
	{
		return BoardSurface;
	}
	AActor* Board = ResolveBoardActor();
	if (!IsValid(Board))
	{
		return nullptr;
	}
	TArray<UPrimitiveComponent*> Components;
	Board->GetComponents<UPrimitiveComponent>(Components);
	float LargestArea = -1.0f;
	for (UPrimitiveComponent* Candidate : Components)
	{
		if (!IsValid(Candidate) || Candidate->ComponentHasTag(TEXT("DiceBoundaryWall")))
		{
			continue;
		}
		const FBoxSphereBounds Bounds = Candidate->CalcBounds(FTransform::Identity);
		const float Area = Bounds.BoxExtent.X * Bounds.BoxExtent.Y;
		if (Area > LargestArea)
		{
			LargestArea = Area;
			BoardSurface = Candidate;
		}
	}
	return BoardSurface;
}

FVector ASpectacleDiceRollManager::GetBoardUpVector() const
{
	return IsValid(BoardSurface)
		? BoardSurface->GetComponentTransform().TransformVectorNoScale(FVector::UpVector).GetSafeNormal()
		: FVector::UpVector;
}

float ASpectacleDiceRollManager::GetBoardClearance(const UPrimitiveComponent* Body) const
{
	if (!IsValid(Body) || !IsValid(BoardSurface))
	{
		return -1.0f;
	}
	const FTransform Transform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = Transform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector BoardTop = Transform.TransformPosition(FVector(Bounds.Origin.X, Bounds.Origin.Y,
		Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z));
	const FVector BoardUp = GetBoardUpVector();
	const float Radius = FVector::DotProduct(Body->Bounds.BoxExtent.GetAbs(), BoardUp.GetAbs());
	return FVector::DotProduct(Body->GetComponentLocation() - BoardTop, BoardUp) - Radius;
}

bool ASpectacleDiceRollManager::RecoverEscapedDie(FActiveDie& State) const
{
	UPrimitiveComponent* Body = State.Body.Get();
	if (!IsValid(Body) || !IsValid(BoardSurface) || GetBoardClearance(Body) >= -EscapedDiceDepth)
	{
		return false;
	}
	const FTransform Transform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = Transform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector BoardUp = GetBoardUpVector();
	const float Radius = FVector::DotProduct(Body->Bounds.BoxExtent.GetAbs(), BoardUp.GetAbs());
	FVector Local = Transform.InverseTransformPosition(Body->GetComponentLocation());
	const float MarginX = (Radius + BoardWallInset + 1.0f) / AbsScale.X;
	const float MarginY = (Radius + BoardWallInset + 1.0f) / AbsScale.Y;
	Local.X = FMath::Clamp(Local.X, Bounds.Origin.X - Bounds.BoxExtent.X + MarginX,
		Bounds.Origin.X + Bounds.BoxExtent.X - MarginX);
	Local.Y = FMath::Clamp(Local.Y, Bounds.Origin.Y - Bounds.BoxExtent.Y + MarginY,
		Bounds.Origin.Y + Bounds.BoxExtent.Y - MarginY);
	Local.Z = Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z
		+ (Radius + MinimumAirborneClearance) / AbsScale.Z;
	Body->SetWorldLocation(Transform.TransformPosition(Local), false, nullptr, ETeleportType::TeleportPhysics);
	Body->SetPhysicsLinearVelocity(BoardUp * 110.0f);
	Body->WakeAllRigidBodies();
	State.bHasBeenAirborne = false;
	State.bHadSupportContact = false;
	return true;
}

void ASpectacleDiceRollManager::SetBoardBoundaryInset(const float EffectiveWallInset,
	UPrimitiveComponent* DiceBody)
{
	if (!IsValid(DiceBody) || !IsValid(ResolveBoardSurface()) || !IsValid(BoardActor))
	{
		return;
	}
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector LocalMin = Bounds.Origin - Bounds.BoxExtent;
	const FVector LocalMax = Bounds.Origin + Bounds.BoxExtent;
	const FVector AbsScale = BoardSurface->GetComponentTransform().GetScale3D().GetAbs()
		.ComponentMax(FVector(0.001f));
	const float HalfThicknessX = BoardWallThickness * 0.5f / AbsScale.X;
	const float HalfThicknessY = BoardWallThickness * 0.5f / AbsScale.Y;
	const float HalfFloorThickness = BoardFloorThickness * 0.5f / AbsScale.Z;
	const float HalfHeight = BoardWallHeight * 0.5f / AbsScale.Z;
	const float InsetX = FMath::Max(EffectiveWallInset, 0.0f) / AbsScale.X;
	const float InsetY = FMath::Max(EffectiveWallInset, 0.0f) / AbsScale.Y;
	const float TopZ = LocalMax.Z - BoardPlayableSurfaceInset / AbsScale.Z;
	const float WallZ = TopZ + HalfHeight;
	const float BoardHalfX = FMath::Max((LocalMax.X - LocalMin.X) * 0.5f, 1.0f);
	const float BoardHalfY = FMath::Max((LocalMax.Y - LocalMin.Y) * 0.5f, 1.0f);
	const ECollisionChannel DiceChannel = DiceBody->GetCollisionObjectType();

	CreateOrUpdateBoardCollider(TEXT("DiceBoundary_Floor"),
		FVector(Bounds.Origin.X, Bounds.Origin.Y, TopZ - HalfFloorThickness),
		FVector(BoardHalfX, BoardHalfY, HalfFloorThickness), DiceChannel);
	CreateOrUpdateBoardCollider(TEXT("DiceBoundary_XMin"),
		FVector(LocalMin.X + InsetX - HalfThicknessX, Bounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight), DiceChannel);
	CreateOrUpdateBoardCollider(TEXT("DiceBoundary_XMax"),
		FVector(LocalMax.X - InsetX + HalfThicknessX, Bounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight), DiceChannel);
	CreateOrUpdateBoardCollider(TEXT("DiceBoundary_YMin"),
		FVector(Bounds.Origin.X, LocalMin.Y + InsetY - HalfThicknessY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight), DiceChannel);
	CreateOrUpdateBoardCollider(TEXT("DiceBoundary_YMax"),
		FVector(Bounds.Origin.X, LocalMax.Y - InsetY + HalfThicknessY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight), DiceChannel);
}

UBoxComponent* ASpectacleDiceRollManager::CreateOrUpdateBoardCollider(const FName ColliderName,
	const FVector& RelativeLocation, const FVector& LocalBoxExtent, const ECollisionChannel DiceChannel)
{
	if (!IsValid(BoardActor) || !IsValid(BoardSurface))
	{
		return nullptr;
	}
	UBoxComponent* Collider = FindObject<UBoxComponent>(BoardActor, *ColliderName.ToString());
	if (!IsValid(Collider))
	{
		Collider = NewObject<UBoxComponent>(BoardActor, ColliderName);
		BoardActor->AddInstanceComponent(Collider);
		Collider->ComponentTags.AddUnique(TEXT("DiceBoundaryWall"));
		Collider->SetupAttachment(BoardSurface);
	}
	Collider->SetRelativeLocation(RelativeLocation);
	Collider->SetRelativeRotation(FRotator::ZeroRotator);
	Collider->SetBoxExtent(LocalBoxExtent, false);
	Collider->SetMobility(BoardSurface->Mobility);
	Collider->SetCollisionObjectType(ECC_WorldStatic);
	Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collider->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collider->SetCollisionResponseToChannel(DiceChannel, ECR_Block);
	Collider->SetGenerateOverlapEvents(false);
	Collider->SetCanEverAffectNavigation(false);
	Collider->SetHiddenInGame(true);
	if (!Collider->IsRegistered())
	{
		Collider->RegisterComponent();
	}
	return Collider;
}
