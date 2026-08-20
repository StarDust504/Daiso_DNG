// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/NaturalDiceRollManager.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Dice/CPP_Dice.h"
#include "Dice/DiceImpactFeedbackBridge.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UnrealType.h"

namespace NaturalDiceRoll
{
	TMap<int32, FVector> MakeFaceNormals()
	{
		// These are derived from the six SetWorldRotation values already calibrated in BP_Dice.
		const TPair<int32, FRotator> LandingRotations[] = {
			{1, FRotator(0.0f, 0.0f, 0.0f)},
			{2, FRotator(0.0f, 0.0f, 90.0f)},
			{3, FRotator(-90.0f, 0.0f, 0.0f)},
			{4, FRotator(90.0f, 0.0f, 0.0f)},
			{5, FRotator(0.0f, 0.0f, -90.0f)},
			{6, FRotator(0.0f, 90.0f, 180.0f)},
		};

		TMap<int32, FVector> Result;
		for (const TPair<int32, FRotator>& Entry : LandingRotations)
		{
			Result.Add(Entry.Key,
				Entry.Value.Quaternion().Inverse().RotateVector(FVector::UpVector).GetSafeNormal());
		}
		return Result;
	}
}

ANaturalDiceRollManager::ANaturalDiceRollManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	FaceLocalNormals = NaturalDiceRoll::MakeFaceNormals();
	NaturalPhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("NaturalDicePhysicalMaterial"));
}

void ANaturalDiceRollManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopActiveRoll(false);
	Super::EndPlay(EndPlayReason);
}

bool ANaturalDiceRollManager::RollAllDice()
{
	if (!GetWorld())
	{
		return false;
	}

	if (bIsRolling)
	{
		StopActiveRoll(false);
	}

	TArray<ACPP_Dice*> DiceActors;
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && IsValid(It->SMC_Dice))
		{
			DiceActors.Add(*It);
		}
	}
	DiceActors.Sort([](const ACPP_Dice& Left, const ACPP_Dice& Right)
	{
		return Left.GetName() < Right.GetName();
	});

	if (DiceActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Natural dice roll found no playable dice actors."));
		return false;
	}

	ResolveBoardSurface();
	EnsureBoardBoundaries(DiceActors[0]->SMC_Dice, BoardWallInset);

	FVector SharedTarget = FVector::ZeroVector;
	if (IsValid(BoardSurface))
	{
		const FTransform BoardTransform = BoardSurface->GetComponentTransform();
		const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
		SharedTarget = BoardTransform.TransformPosition(FVector(
			Bounds.Origin.X + FMath::FRandRange(-0.12f, 0.12f) * Bounds.BoxExtent.X,
			Bounds.Origin.Y + FMath::FRandRange(-0.12f, 0.12f) * Bounds.BoxExtent.Y,
			Bounds.Origin.Z + Bounds.BoxExtent.Z));
	}
	else
	{
		for (const ACPP_Dice* Dice : DiceActors)
		{
			SharedTarget += Dice->GetActorLocation();
		}
		SharedTarget /= DiceActors.Num();
	}

	ActiveDice.Reserve(DiceActors.Num());
	LastRollReboundCount = 0;
	LastRollImpactFeedbackCount = 0;
	if (IsValid(NaturalPhysicalMaterial))
	{
		NaturalPhysicalMaterial->Restitution = FMath::Clamp(DiceRestitution, 0.0f, 1.0f);
		NaturalPhysicalMaterial->Friction = FMath::Max(DiceFriction, 0.0f);
		NaturalPhysicalMaterial->StaticFriction = FMath::Max(DiceFriction * 1.10f, 0.0f);
		NaturalPhysicalMaterial->bOverrideRestitutionCombineMode = true;
		NaturalPhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Max;
		NaturalPhysicalMaterial->bOverrideFrictionCombineMode = true;
		NaturalPhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Average;
	}
	for (ACPP_Dice* Dice : DiceActors)
	{
		if (UDicePhysicsRollComponent* GuidedRoll = Dice->FindComponentByClass<UDicePhysicsRollComponent>();
			IsValid(GuidedRoll) && GuidedRoll->IsRolling())
		{
			GuidedRoll->CancelRoll(true);
		}

		UPrimitiveComponent* Body = Dice->SMC_Dice;
		FActiveDie& State = ActiveDice.AddDefaulted_GetRef();
		State.Dice = Dice;
		State.Body = Body;
		State.FeedbackSource = Dice->FindComponentByClass<UDicePhysicsRollComponent>();
		State.OriginalAttachParent = Body->GetAttachParent();
		State.OriginalAttachSocket = Body->GetAttachSocketName();
		State.OriginalPhysicalMaterial = Body->BodyInstance.GetSimplePhysicalMaterial();
		State.OriginalLinearDamping = Body->GetLinearDamping();
		State.OriginalAngularDamping = Body->GetAngularDamping();
		State.OriginalWorldDynamicResponse = Body->GetCollisionResponseToChannel(ECC_WorldDynamic);
		State.bOriginalSimulatePhysics = Body->IsSimulatingPhysics();
		State.bOriginalGravity = Body->IsGravityEnabled();
		State.bOriginalNotifyRigidBodyCollision = Body->BodyInstance.bNotifyRigidBodyCollision;
		State.bOriginalUseCCD = Body->BodyInstance.bUseCCD;

		Dice->SetIsActive(false);
		SetGeneratedNumber(Dice, 0);
		Body->OnComponentHit.RemoveDynamic(this, &ANaturalDiceRollManager::HandleDiceHit);
		Body->OnComponentHit.AddDynamic(this, &ANaturalDiceRollManager::HandleDiceHit);
		Body->SetNotifyRigidBodyCollision(true);
		Body->SetUseCCD(true);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		if (IsValid(NaturalPhysicalMaterial))
		{
			Body->SetPhysMaterialOverride(NaturalPhysicalMaterial);
		}
		Body->SetLinearDamping(0.04f);
		Body->SetAngularDamping(0.035f);
		Body->SetEnableGravity(true);
		Body->SetSimulatePhysics(true);
		Body->WakeAllRigidBodies();

		FVector ToCollisionPoint = SharedTarget - Body->GetComponentLocation();
		ToCollisionPoint.Z = 0.0f;
		ToCollisionPoint = ToCollisionPoint.GetSafeNormal();
		FVector RandomDirection = FMath::VRand();
		RandomDirection.Z = 0.0f;
		RandomDirection = RandomDirection.GetSafeNormal();
		if (ToCollisionPoint.IsNearlyZero())
		{
			ToCollisionPoint = RandomDirection;
		}
		const FVector HorizontalDirection = FMath::Lerp(
			ToCollisionPoint, RandomDirection, FMath::Clamp(DirectionJitter, 0.0f, 1.0f)).GetSafeNormal();
		const float HorizontalSpeed = FMath::FRandRange(
			FMath::Min(HorizontalSpeedMin, HorizontalSpeedMax),
			FMath::Max(HorizontalSpeedMin, HorizontalSpeedMax));
		const float UpwardSpeed = FMath::FRandRange(
			FMath::Min(UpwardSpeedMin, UpwardSpeedMax), FMath::Max(UpwardSpeedMin, UpwardSpeedMax));
		Body->AddImpulse(HorizontalDirection * HorizontalSpeed + GetBoardUpVector() * UpwardSpeed,
			NAME_None, true);

		FVector SpinAxis = FMath::VRand().GetSafeNormal();
		if (FMath::Abs(FVector::DotProduct(SpinAxis, GetBoardUpVector())) > 0.82f)
		{
			SpinAxis = (SpinAxis + FMath::VRand() * 0.55f).GetSafeNormal();
		}
		const float SpinSpeed = FMath::FRandRange(
			FMath::Min(SpinSpeedMin, SpinSpeedMax), FMath::Max(SpinSpeedMin, SpinSpeedMax));
		Body->AddAngularImpulseInRadians(SpinAxis * SpinSpeed, NAME_None, true);
	}

	RollElapsed = 0.0f;
	GroupStableElapsed = 0.0f;
	bIsRolling = true;
	SetActorTickEnabled(true);
	OnNaturalRollStarted.Broadcast();
	UE_LOG(LogTemp, Display, TEXT("Natural dice roll launched %d dice; results are intentionally unknown."),
		ActiveDice.Num());
	return true;
}

void ANaturalDiceRollManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bIsRolling)
	{
		return;
	}

	RollElapsed += DeltaSeconds;
	bool bAllSettled = RollElapsed >= 0.70f;
	for (FActiveDie& State : ActiveDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			bAllSettled = false;
			continue;
		}

		if (GetBoardClearance(Body) >= MinimumAirborneClearance)
		{
			State.bHasBeenAirborne = true;
		}
		RecoverEscapedDie(State);
		const float UpwardVelocity = FVector::DotProduct(
			Body->GetPhysicsLinearVelocity(), GetBoardUpVector());
		if (State.bAwaitingRebound && UpwardVelocity >= 35.0f)
		{
			State.bAwaitingRebound = false;
			State.bObservedRebound = true;
		}

		if (RollElapsed >= SettleDampingDelay)
		{
			const float DampingAlpha = FMath::Clamp((RollElapsed - SettleDampingDelay) / 1.5f, 0.0f, 1.0f);
			Body->SetLinearDamping(FMath::Lerp(0.04f, 1.35f, DampingAlpha));
			Body->SetAngularDamping(FMath::Lerp(0.035f, 4.5f, DampingAlpha));
		}

		const bool bSlow = Body->GetPhysicsLinearVelocity().Size() <= SettleLinearSpeed
			&& Body->GetPhysicsAngularVelocityInRadians().Size() <= SettleAngularSpeed;
		bAllSettled = bAllSettled && State.bHadSupportContact && bSlow;
	}

	GroupStableElapsed = bAllSettled ? GroupStableElapsed + DeltaSeconds : 0.0f;
	if (GroupStableElapsed >= RequiredGroupStableTime || RollElapsed >= MaximumRollTime)
	{
		if (RollElapsed >= MaximumRollTime && !bAllSettled)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Natural dice roll reached its %.1fs safety timeout; reading the final physical orientations."),
				MaximumRollTime);
		}
		StopActiveRoll(true);
	}
}

void ANaturalDiceRollManager::HandleDiceHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bIsRolling || !IsValid(HitComponent))
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
	const bool bHitBoard = OtherActor == BoardActor
		|| OtherComponent == BoardSurface
		|| (IsValid(OtherComponent) && OtherComponent->ComponentHasTag(TEXT("DiceBoundaryWall")));
	const bool bHitUpwardFacingSupport = FVector::DotProduct(
		Hit.ImpactNormal.GetSafeNormal(), BoardUp) >= 0.50f;
	const bool bLaunchContact = bHitBoard && bHitUpwardFacingSupport && !State->bHasBeenAirborne;
	if (bHitBoard && !bLaunchContact && DiceImpactFeedbackBridge::PlayMatchingGuidedImpact(
		State->FeedbackSource.Get(), HitComponent, NormalImpulse, Hit, State->LastImpactFeedbackTime))
	{
		++LastRollImpactFeedbackCount;
	}
	if (State->bHasBeenAirborne && bHitUpwardFacingSupport)
	{
		State->bHadSupportContact = true;
		State->bAwaitingRebound = true;
	}

	const bool bHitAnotherDice = IsValid(OtherComponent)
		&& OtherComponent->IsSimulatingPhysics()
		&& ActiveDice.ContainsByPredicate([OtherComponent](const FActiveDie& Candidate)
		{
			return Candidate.Body.Get() == OtherComponent;
		});
	if (!bHitAnotherDice || CollisionAngularKick <= 0.0f || !GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (State->LastCollisionKickTime >= 0.0 && Now - State->LastCollisionKickTime < 0.08)
	{
		return;
	}
	State->LastCollisionKickTime = Now;

	FVector KickAxis = FVector::CrossProduct(Hit.ImpactNormal, BoardUp).GetSafeNormal();
	KickAxis = (KickAxis + FMath::VRand() * 0.75f).GetSafeNormal();
	HitComponent->AddAngularImpulseInRadians(KickAxis * CollisionAngularKick, NAME_None, true);
}

void ANaturalDiceRollManager::StopActiveRoll(const bool bPublishResults)
{
	if (ActiveDice.IsEmpty())
	{
		bIsRolling = false;
		SetActorTickEnabled(false);
		return;
	}

	TArray<int32> Results;
	Results.Reserve(ActiveDice.Num());
	LastRollReboundCount = 0;
	UPrimitiveComponent* BoundaryChannelBody = nullptr;
	for (FActiveDie& State : ActiveDice)
	{
		if (!IsValid(BoundaryChannelBody))
		{
			BoundaryChannelBody = State.Body.Get();
		}
		LastRollReboundCount += State.bObservedRebound ? 1 : 0;
		ACPP_Dice* Dice = State.Dice.Get();
		UPrimitiveComponent* Body = State.Body.Get();
		int32 Result = 0;
		float Alignment = 0.0f;
		if (bPublishResults && IsValid(Body))
		{
			// This is the first point at which a face value is chosen.
			Result = DetermineTopFaceForBody(Body, &Alignment);
			Results.Add(Result);
			SetGeneratedNumber(Dice, Result);
			OnNaturalDieSettled.Broadcast(Dice, Result);
			UE_LOG(LogTemp, Display, TEXT("Natural dice %s settled on %d (up alignment %.3f)."),
				*GetNameSafe(Dice), Result, Alignment);
		}

		RestoreBody(State);
		if (bPublishResults && IsValid(Dice))
		{
			// Reuse the existing public completion signal so the untouched score collector also receives natural results.
			if (UDicePhysicsRollComponent* GuidedRoll = Dice->FindComponentByClass<UDicePhysicsRollComponent>())
			{
				GuidedRoll->OnDiceRollFinished.Broadcast(Result);
			}
		}
	}
	// The tighter wall is specific to the natural mode. Put the shared colliders back on the old outer
	// bounds after the dice have frozen so the guided button keeps its original playable area.
	EnsureBoardBoundaries(BoundaryChannelBody, 0.0f);

	ActiveDice.Reset();
	bIsRolling = false;
	RollElapsed = 0.0f;
	GroupStableElapsed = 0.0f;
	SetActorTickEnabled(false);
	if (bPublishResults)
	{
		UE_LOG(LogTemp, Display, TEXT("Natural dice roll observed %d/%d physical rebounds."),
			LastRollReboundCount, Results.Num());
		OnNaturalRollFinished.Broadcast(Results);
	}
}

void ANaturalDiceRollManager::RestoreBody(FActiveDie& State)
{
	UPrimitiveComponent* Body = State.Body.Get();
	if (!IsValid(Body))
	{
		return;
	}

	Body->OnComponentHit.RemoveDynamic(this, &ANaturalDiceRollManager::HandleDiceHit);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Body->SetLinearDamping(State.OriginalLinearDamping);
	Body->SetAngularDamping(State.OriginalAngularDamping);
	Body->SetNotifyRigidBodyCollision(State.bOriginalNotifyRigidBodyCollision);
	Body->SetUseCCD(State.bOriginalUseCCD);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, State.OriginalWorldDynamicResponse);
	Body->SetPhysMaterialOverride(State.OriginalPhysicalMaterial.Get());
	Body->SetEnableGravity(State.bOriginalGravity);
	Body->SetSimulatePhysics(State.bOriginalSimulatePhysics);
	if (!State.bOriginalSimulatePhysics && State.OriginalAttachParent.IsValid())
	{
		Body->AttachToComponent(State.OriginalAttachParent.Get(), FAttachmentTransformRules::KeepWorldTransform,
			State.OriginalAttachSocket);
	}
}

void ANaturalDiceRollManager::SetGeneratedNumber(AActor* DiceActor, const int32 Result) const
{
	if (!IsValid(DiceActor))
	{
		return;
	}

	if (FIntProperty* NumberProperty = FindFProperty<FIntProperty>(DiceActor->GetClass(), TEXT("GeneratedNumber")))
	{
		NumberProperty->SetPropertyValue_InContainer(DiceActor, Result);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Natural dice roll could not find GeneratedNumber on %s."),
			*GetNameSafe(DiceActor));
	}
}

int32 ANaturalDiceRollManager::DetermineTopFaceForBody(const UPrimitiveComponent* Body, float* OutAlignment) const
{
	if (!IsValid(Body))
	{
		if (OutAlignment)
		{
			*OutAlignment = -1.0f;
		}
		return 0;
	}

	int32 BestFace = 0;
	float BestDot = -2.0f;
	const FVector BoardUp = GetBoardUpVector();
	for (const TPair<int32, FVector>& Entry : FaceLocalNormals)
	{
		const FVector WorldNormal = Body->GetComponentQuat().RotateVector(Entry.Value.GetSafeNormal());
		const float Dot = FVector::DotProduct(WorldNormal, BoardUp);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestFace = Entry.Key;
		}
	}
	if (OutAlignment)
	{
		*OutAlignment = BestDot;
	}
	return BestFace;
}

int32 ANaturalDiceRollManager::DetermineTopFace(const FQuat& WorldRotation, const FVector& WorldUp,
	float* OutAlignment)
{
	const TMap<int32, FVector> Normals = NaturalDiceRoll::MakeFaceNormals();
	const FVector SafeUp = WorldUp.GetSafeNormal();
	int32 BestFace = 0;
	float BestDot = -2.0f;
	for (const TPair<int32, FVector>& Entry : Normals)
	{
		const float Dot = FVector::DotProduct(
			WorldRotation.RotateVector(Entry.Value).GetSafeNormal(), SafeUp);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestFace = Entry.Key;
		}
	}
	if (OutAlignment)
	{
		*OutAlignment = BestDot;
	}
	return BestFace;
}

AActor* ANaturalDiceRollManager::ResolveBoardActor()
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

UPrimitiveComponent* ANaturalDiceRollManager::ResolveBoardSurface()
{
	if (IsValid(BoardSurface))
	{
		return BoardSurface;
	}

	AActor* ResolvedBoard = ResolveBoardActor();
	if (!IsValid(ResolvedBoard))
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> Components;
	ResolvedBoard->GetComponents<UPrimitiveComponent>(Components);
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

void ANaturalDiceRollManager::EnsureBoardBoundaries(UPrimitiveComponent* DiceBody,
	const float EffectiveWallInset)
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

	CreateBoardCollider(TEXT("DiceBoundary_Floor"),
		FVector(Bounds.Origin.X, Bounds.Origin.Y, TopZ - HalfFloorThickness),
		FVector(BoardHalfX, BoardHalfY, HalfFloorThickness), DiceChannel);
	CreateBoardCollider(TEXT("DiceBoundary_XMin"), FVector(LocalMin.X + InsetX - HalfThicknessX, Bounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight), DiceChannel);
	CreateBoardCollider(TEXT("DiceBoundary_XMax"), FVector(LocalMax.X - InsetX + HalfThicknessX, Bounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight), DiceChannel);
	CreateBoardCollider(TEXT("DiceBoundary_YMin"), FVector(Bounds.Origin.X, LocalMin.Y + InsetY - HalfThicknessY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight), DiceChannel);
	CreateBoardCollider(TEXT("DiceBoundary_YMax"), FVector(Bounds.Origin.X, LocalMax.Y - InsetY + HalfThicknessY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight), DiceChannel);
}

UBoxComponent* ANaturalDiceRollManager::CreateBoardCollider(const FName ColliderName,
	const FVector& RelativeLocation, const FVector& LocalBoxExtent, const ECollisionChannel DiceChannel)
{
	if (!IsValid(BoardActor) || !IsValid(BoardSurface))
	{
		return nullptr;
	}
	UBoxComponent* Collider = FindObject<UBoxComponent>(BoardActor, *ColliderName.ToString());
	const bool bNewCollider = !IsValid(Collider);
	if (bNewCollider)
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

FVector ANaturalDiceRollManager::GetBoardUpVector() const
{
	return IsValid(BoardSurface)
		? BoardSurface->GetComponentTransform().TransformVectorNoScale(FVector::UpVector).GetSafeNormal()
		: FVector::UpVector;
}

float ANaturalDiceRollManager::GetBoardClearance(const UPrimitiveComponent* Body) const
{
	if (!IsValid(Body) || !IsValid(BoardSurface))
	{
		return -1.0f;
	}

	const FTransform BoardTransform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = BoardTransform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector BoardTop = BoardTransform.TransformPosition(FVector(Bounds.Origin.X, Bounds.Origin.Y,
		Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z));
	const FVector BoardUp = GetBoardUpVector();
	const float BodyRadius = FVector::DotProduct(Body->Bounds.BoxExtent.GetAbs(), BoardUp.GetAbs());
	return FVector::DotProduct(Body->GetComponentLocation() - BoardTop, BoardUp) - BodyRadius;
}

bool ANaturalDiceRollManager::RecoverEscapedDie(FActiveDie& State) const
{
	UPrimitiveComponent* Body = State.Body.Get();
	if (!IsValid(Body) || !IsValid(BoardSurface) || GetBoardClearance(Body) >= -EscapedDiceDepth)
	{
		return false;
	}

	const FTransform BoardTransform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = BoardTransform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector BoardUp = GetBoardUpVector();
	const float BodyRadius = FVector::DotProduct(Body->Bounds.BoxExtent.GetAbs(), BoardUp.GetAbs());
	FVector Local = BoardTransform.InverseTransformPosition(Body->GetComponentLocation());
	const float MarginX = (BodyRadius + BoardWallInset + 1.0f) / AbsScale.X;
	const float MarginY = (BodyRadius + BoardWallInset + 1.0f) / AbsScale.Y;
	Local.X = FMath::Clamp(Local.X, Bounds.Origin.X - Bounds.BoxExtent.X + MarginX,
		Bounds.Origin.X + Bounds.BoxExtent.X - MarginX);
	Local.Y = FMath::Clamp(Local.Y, Bounds.Origin.Y - Bounds.BoxExtent.Y + MarginY,
		Bounds.Origin.Y + Bounds.BoxExtent.Y - MarginY);
	Local.Z = Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z
		+ (BodyRadius + MinimumAirborneClearance) / AbsScale.Z;
	Body->SetWorldLocation(BoardTransform.TransformPosition(Local), false, nullptr, ETeleportType::TeleportPhysics);
	Body->SetPhysicsLinearVelocity(BoardUp * 105.0f, false);
	Body->WakeAllRigidBodies();
	State.bHasBeenAirborne = false;
	State.bHadSupportContact = false;
	return true;
}
