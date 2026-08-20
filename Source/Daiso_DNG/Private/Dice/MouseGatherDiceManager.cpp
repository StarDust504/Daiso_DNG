// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/MouseGatherDiceManager.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Dice/CPP_Dice.h"
#include "Dice/DiceImpactFeedbackBridge.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "Dice/NaturalDiceRollManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UnrealType.h"

AMouseGatherDiceManager::AMouseGatherDiceManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	DropPhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("MouseGatherDropPhysicalMaterial"));
}

void AMouseGatherDiceManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FGatheredDie& State : GatheredDice)
	{
		RestoreGuidedSettings(State);
		RestoreDie(State, true);
	}
	if (!GatheredDice.IsEmpty())
	{
		SetBoardBoundaryInset(0.0f, GatheredDice[0].Body.Get());
	}
	GatheredDice.Reset();
	Super::EndPlay(EndPlayReason);
}

bool AMouseGatherDiceManager::BeginGather(const EMouseGatherDropMode NewMode)

{
	return BeginGatherInternal(NewMode, false);
}

bool AMouseGatherDiceManager::BeginTrajectoryGather(const EMouseGatherDropMode NewMode)
{
	return BeginGatherInternal(NewMode, true);
}

/** Общая инициализация сохраняет прежний drop-вариант и добавляет независимый жестовый вариант. */
bool AMouseGatherDiceManager::BeginGatherInternal(
	const EMouseGatherDropMode NewMode, const bool bUseTrajectory)
{
	if (IsDropping() || !GetWorld())
	{
		return false;
	}

	const bool bStartingNewGather = InteractionState != EMouseGatherState::Gathering;
	LastFinishedDice.Reset();
	DropMode = NewMode;
	bUseTrajectoryGesture = bUseTrajectory;
	bTrajectoryGestureActive = false;
	SmoothedGestureVelocity = FVector::ZeroVector;
	InteractionState = EMouseGatherState::Gathering;
	if (bStartingNewGather)
	{
		CarryElapsed = 0.0f;
		CarryAnchor = FVector::ZeroVector;
		bHasCarryAnchor = false;
	}
	bWaitForActivationClickRelease = true;
	bPreviousLeftMouseDown = true;
	ResolveBoardSurface();
	SetActorTickEnabled(true);
	PublishStatus(bUseTrajectory
		? NSLOCTEXT("MouseGatherDice", "TrajectoryGatherStarted",
			"Проведите курсором по нужным кубикам. Затем удерживайте ЛКМ, сделайте бросковый жест и отпустите.")
		: NewMode == EMouseGatherDropMode::Natural
		? NSLOCTEXT("MouseGatherDice", "NaturalGatherStarted",
			"Проведите мышью по кубикам. ЛКМ — уронить; числа определятся после остановки.")
		: NSLOCTEXT("MouseGatherDice", "PredictedGatherStarted",
			"Проведите мышью по кубикам. ЛКМ — уронить с заранее выбранными гранями."));
	return true;
}

bool AMouseGatherDiceManager::TryGatherDice(AActor* DiceActor)
{
	if (InteractionState != EMouseGatherState::Gathering)
	{
		return false;
	}

	ACPP_Dice* Dice = Cast<ACPP_Dice>(DiceActor);
	if (!IsValid(Dice) || !IsValid(Dice->SMC_Dice) || !Dice->GetCanRollDice()
		|| Dice->GetIsActive() || Dice->bIsHidden
		|| GatheredDice.ContainsByPredicate([Dice](const FGatheredDie& State)
		{
			return State.Dice.Get() == Dice;
		}))
	{
		return false;
	}

	UDicePhysicsRollComponent* GuidedRoll = Dice->FindComponentByClass<UDicePhysicsRollComponent>();
	if (IsValid(GuidedRoll) && GuidedRoll->IsRolling())
	{
		GuidedRoll->CancelRoll(true);
	}

	UPrimitiveComponent* Body = Dice->SMC_Dice;
	FGatheredDie& State = GatheredDice.AddDefaulted_GetRef();
	State.Dice = Dice;
	State.Body = Body;
	State.GuidedRoll = GuidedRoll;
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
	State.bOriginalCanRoll = Dice->GetCanRollDice();
	State.CarryPhase = static_cast<float>(GatheredDice.Num() - 1) * 1.6180339f;
	State.CarrySpinAxis = FVector(
		FMath::Sin(State.CarryPhase + 0.7f),
		FMath::Cos(State.CarryPhase * 1.31f + 0.2f),
		FMath::Sin(State.CarryPhase * 0.73f + 1.1f)).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);

	Dice->SetCanRollDice(false);
	Dice->SetIsActive(false);
	SetGeneratedNumber(Dice, 0);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Body->SetEnableGravity(false);
	Body->SetSimulatePhysics(false);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ResolveBoardSurface();
	if (IsValid(BoardSurface))
	{
		// Give immediate visual pickup feedback even before the cursor moves on its next frame.
		const float LiftDistance = FMath::Max(HoverHeight - GetBoardClearance(Body), 0.0f);
		Body->SetWorldLocation(Body->GetComponentLocation() + GetBoardUpVector() * LiftDistance,
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (!bHasCarryAnchor)
	{
		CarryAnchor = Body->GetComponentLocation();
		bHasCarryAnchor = true;
	}

	// Keep the carried group genuinely physical: the dice collide and tumble while soft springs hold the cloud together.
	Body->SetLinearDamping(0.16f);
	Body->SetAngularDamping(0.18f);
	Body->SetUseCCD(true);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();

	PublishStatus(FText::Format(
		NSLOCTEXT("MouseGatherDice", "DiceGathered", "Собрано кубиков: {0}. Проведите по другим или нажмите ЛКМ."),
		FText::AsNumber(GatheredDice.Num())));
	return true;
}

bool AMouseGatherDiceManager::DropGatheredDice()

{
	return LaunchGatheredDice(FVector::ZeroVector);
}

/** Выпускает группу с общей скоростью, сохраняя honest/predicted семантику старого пути. */
bool AMouseGatherDiceManager::LaunchGatheredDice(const FVector ThrowVelocity)
{
	if (InteractionState != EMouseGatherState::Gathering)
	{
		return false;
	}
	if (GatheredDice.IsEmpty())
	{
		PublishStatus(NSLOCTEXT("MouseGatherDice", "NothingGathered",
			"Сначала проведите курсором хотя бы по одному кубику."));
		return false;
	}

	if (IsValid(DropPhysicalMaterial))
	{
		DropPhysicalMaterial->Restitution = FMath::Clamp(DiceRestitution, 0.0f, 1.0f);
		DropPhysicalMaterial->Friction = FMath::Max(DiceFriction, 0.0f);
		DropPhysicalMaterial->StaticFriction = FMath::Max(DiceFriction * 1.1f, 0.0f);
		DropPhysicalMaterial->bOverrideRestitutionCombineMode = true;
		DropPhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Max;
		DropPhysicalMaterial->bOverrideFrictionCombineMode = true;
		DropPhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Average;
	}

	SetBoardBoundaryInset(BoardWallInset, GatheredDice[0].Body.Get());
	DropElapsed = 0.0f;
	StableElapsed = 0.0f;
	if (DropMode == EMouseGatherDropMode::Natural)
	{
		InteractionState = EMouseGatherState::NaturalDropping;
		LastNaturalDropImpactFeedbackCount = 0;
		for (FGatheredDie& State : GatheredDice)
		{
			UPrimitiveComponent* Body = State.Body.Get();
			if (!IsValid(Body))
			{
				continue;
			}
			State.Result = 0;
			State.bHadSupportContact = false;
			Body->OnComponentHit.RemoveDynamic(this, &AMouseGatherDiceManager::HandleNaturalDropHit);
			Body->OnComponentHit.AddDynamic(this, &AMouseGatherDiceManager::HandleNaturalDropHit);
			Body->SetNotifyRigidBodyCollision(true);
			Body->SetUseCCD(true);
			Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
			Body->SetPhysMaterialOverride(DropPhysicalMaterial);
			Body->SetLinearDamping(0.04f);
			Body->SetAngularDamping(0.035f);
			Body->SetEnableGravity(true);
			Body->SetSimulatePhysics(true);
			Body->WakeAllRigidBodies();
			if (!ThrowVelocity.IsNearlyZero())
			{
				Body->SetPhysicsLinearVelocity(ThrowVelocity);
			}

			FVector Horizontal = FMath::VRand();
			Horizontal.Z = 0.0f;
			Body->AddImpulse(Horizontal.GetSafeNormal() * FMath::FRandRange(
				NaturalHorizontalKick * 0.45f, NaturalHorizontalKick), NAME_None, true);
			Body->AddAngularImpulseInRadians(FMath::VRand().GetSafeNormal()
				* FMath::FRandRange(NaturalSpinSpeed * 0.75f, NaturalSpinSpeed), NAME_None, true);
		}
		PublishStatus(!ThrowVelocity.IsNearlyZero()
			? NSLOCTEXT("MouseGatherDice", "NaturalThrown",
				"Пригоршня брошена по траектории: результат пока неизвестен…")
			: NSLOCTEXT("MouseGatherDice", "NaturalDropped",
				"Кубики отпущены: результат пока неизвестен…"));
	}
	else
	{
		InteractionState = EMouseGatherState::PredictedDropping;
		for (FGatheredDie& State : GatheredDice)
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
			GuidedRoll->HorizontalSpeed = PredictedHorizontalSpeed;
			GuidedRoll->SpinSpeed = PredictedSpinSpeed;
			GuidedRoll->AirLinearDamping = 0.04f;
			GuidedRoll->AirAngularDamping = 0.035f;
			GuidedRoll->FreeFlightTime = 0.08f;
			GuidedRoll->AerialAlignmentLeadTime = 0.68f;
			GuidedRoll->OrientationStrength = 68.0f;
			GuidedRoll->OrientationDamping = 8.5f;
			GuidedRoll->MaxAngularAcceleration = 165.0f;
			GuidedRoll->LandingLinearDamping = 1.25f;
			GuidedRoll->LandingAngularDamping = 7.5f;
			GuidedRoll->RequiredStableTime = 0.22f;
			GuidedRoll->MaximumRollTime = 5.0f;
			GuidedRoll->bAssistOnlyWhileFalling = true;
			GuidedRoll->bUseAirborneSafetyAlignment = true;
			GuidedRoll->bRetryWrongFaceWithBounce = true;
			GuidedRoll->bFreezeAfterLanding = true;
			GuidedRoll->PhysicalMaterialOverride = DropPhysicalMaterial;

			State.Result = FMath::RandRange(1, 6);
			SetGeneratedNumber(State.Dice.Get(), State.Result);
			GuidedRoll->RollToRotation(State.Result, GetLandingRotation(State.Result));
		}
		PublishStatus(NSLOCTEXT("MouseGatherDice", "PredictedDropped",
			"Кубики отпущены: выбранные грани направляются физикой…"));
	}
	return true;
}

void AMouseGatherDiceManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	switch (InteractionState)
	{
	case EMouseGatherState::Gathering:
		UpdateGathering(DeltaSeconds);
		break;
	case EMouseGatherState::NaturalDropping:
		UpdateNaturalDrop(DeltaSeconds);
		break;
	case EMouseGatherState::PredictedDropping:
		UpdatePredictedDrop();
		break;
	default:
		break;
	}
}

void AMouseGatherDiceManager::UpdateGathering(const float DeltaSeconds)
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!bTrajectoryGestureActive)
	{
		TryGatherUnderCursor();
	}
	CarryElapsed += DeltaSeconds;
	FVector CursorPoint;
	if (GetCursorBoardPoint(CursorPoint))
	{
		CarryAnchor = CursorPoint;
		bHasCarryAnchor = true;
	}
	if (bHasCarryAnchor)
	{
		for (int32 Index = 0; Index < GatheredDice.Num(); ++Index)
		{
			FGatheredDie& State = GatheredDice[Index];
			UPrimitiveComponent* Body = State.Body.Get();
			if (!IsValid(Body))
			{
				continue;
			}

			const FVector Target = CarryAnchor
				+ GetFormationOffset(Index, GatheredDice.Num())
				+ GetFloatingOffset(State);
			FVector CarryAcceleration = (Target - Body->GetComponentLocation()) * CarrySpringStrength
				- Body->GetPhysicsLinearVelocity() * CarrySpringDamping;
			CarryAcceleration = CarryAcceleration.GetClampedToMaxSize(9000.0f);
			Body->AddForce(CarryAcceleration, NAME_None, true);

			const FVector DesiredAngularVelocity = State.CarrySpinAxis * CarryTumbleSpeed;
			FVector AngularAcceleration = (DesiredAngularVelocity
				- Body->GetPhysicsAngularVelocityInRadians()) * 4.0f;
			AngularAcceleration = AngularAcceleration.GetClampedToMaxSize(12.0f);
			Body->AddTorqueInRadians(AngularAcceleration, NAME_None, true);
		}
	}

	const bool bLeftDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (bWaitForActivationClickRelease)
	{
		if (!bLeftDown)
		{
			bWaitForActivationClickRelease = false;
		}
	}
	else if (bUseTrajectoryGesture)
	{
		if (!bTrajectoryGestureActive && bLeftDown && !bPreviousLeftMouseDown)
		{
			if (GatheredDice.IsEmpty())
			{
				PublishStatus(NSLOCTEXT("MouseGatherDice", "TrajectoryNothingGathered",
					"Сначала проведите курсором хотя бы по одному кубику."));
			}
			else
			{
				bTrajectoryGestureActive = true;
				SmoothedGestureVelocity = FVector::ZeroVector;
				PreviousGesturePoint = CarryAnchor;
				PublishStatus(NSLOCTEXT("MouseGatherDice", "TrajectoryGesture",
					"Ведите пригоршню с зажатой ЛКМ и отпустите, чтобы бросить."));
			}
		}
		else if (bTrajectoryGestureActive && bLeftDown)
		{
			FVector CurrentPoint;
			if (GetCursorBoardPoint(CurrentPoint) && DeltaSeconds > UE_SMALL_NUMBER)
			{
				const FVector InstantVelocity = (CurrentPoint - PreviousGesturePoint) / DeltaSeconds;
				SmoothedGestureVelocity = FMath::Lerp(SmoothedGestureVelocity, InstantVelocity, 0.38f);
				PreviousGesturePoint = CurrentPoint;
			}
		}
		else if (bTrajectoryGestureActive && !bLeftDown && bPreviousLeftMouseDown)
		{
			FVector ThrowVelocity = SmoothedGestureVelocity * TrajectoryVelocityScale;
			ThrowVelocity = ThrowVelocity.GetClampedToMaxSize(FMath::Max(TrajectoryMaximumSpeed, 10.0f));
			ThrowVelocity += GetBoardUpVector() * FMath::Max(TrajectoryUpwardSpeed, 0.0f);
			bTrajectoryGestureActive = false;
			LaunchGatheredDice(ThrowVelocity);
		}
	}
	else if (bLeftDown && !bPreviousLeftMouseDown)
	{
		DropGatheredDice();
	}
	bPreviousLeftMouseDown = bLeftDown;
}

void AMouseGatherDiceManager::TryGatherUnderCursor()
{
	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController))
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	ACPP_Dice* ClosestDice = nullptr;
	float ClosestDistanceSquared = FMath::Square(PickupScreenRadius);
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		ACPP_Dice* Dice = *It;
		if (!IsValid(Dice) || GatheredDice.ContainsByPredicate([Dice](const FGatheredDie& State)
			{
				return State.Dice.Get() == Dice;
			}))
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Dice->GetActorLocation(), ScreenPosition, true))
		{
			continue;
		}
		const float DistanceSquared = FVector2D::DistSquared(ScreenPosition, FVector2D(MouseX, MouseY));
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestDice = Dice;
		}
	}
	if (IsValid(ClosestDice))
	{
		TryGatherDice(ClosestDice);
	}
}

bool AMouseGatherDiceManager::GetCursorBoardPoint(FVector& OutPoint) const
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

	const FTransform BoardTransform = BoardSurface->GetComponentTransform();
	const FBoxSphereBounds Bounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector AbsScale = BoardTransform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const FVector HoverPlanePoint = BoardTransform.TransformPosition(FVector(
		Bounds.Origin.X, Bounds.Origin.Y,
		Bounds.Origin.Z + Bounds.BoxExtent.Z - BoardPlayableSurfaceInset / AbsScale.Z))
		+ GetBoardUpVector() * HoverHeight;
	const FPlane HoverPlane(HoverPlanePoint, GetBoardUpVector());
	FVector WorldPoint = FMath::LinePlaneIntersection(RayOrigin, RayOrigin + RayDirection * 100000.0f, HoverPlane);
	FVector LocalPoint = BoardTransform.InverseTransformPosition(WorldPoint);
	const float ClampInsetX = (BoardWallInset + GatherSpacing) / AbsScale.X;
	const float ClampInsetY = (BoardWallInset + GatherSpacing) / AbsScale.Y;
	LocalPoint.X = FMath::Clamp(LocalPoint.X,
		Bounds.Origin.X - Bounds.BoxExtent.X + ClampInsetX,
		Bounds.Origin.X + Bounds.BoxExtent.X - ClampInsetX);
	LocalPoint.Y = FMath::Clamp(LocalPoint.Y,
		Bounds.Origin.Y - Bounds.BoxExtent.Y + ClampInsetY,
		Bounds.Origin.Y + Bounds.BoxExtent.Y - ClampInsetY);
	WorldPoint = BoardTransform.TransformPosition(LocalPoint);
	OutPoint = WorldPoint;
	return !OutPoint.ContainsNaN();
}

FVector AMouseGatherDiceManager::GetFormationOffset(const int32 Index, const int32 DiceCount) const
{
	if (Index < 0 || DiceCount <= 1 || !IsValid(BoardSurface))
	{
		return FVector::ZeroVector;
	}

	// Six vertices of a tilted octahedron make a compact ball instead of a flat ring.
	// Subtracting the active vertices' centroid keeps the group centred as dice are added.
	static const FVector Slots[] =
	{
		FVector( 0.9397f,  0.0000f,  0.3420f),
		FVector(-0.9397f,  0.0000f, -0.3420f),
		FVector(-0.1170f,  0.9400f,  0.3210f),
		FVector( 0.1170f, -0.9400f, -0.3210f),
		FVector(-0.3215f, -0.3416f,  0.8832f),
		FVector( 0.3215f,  0.3416f, -0.8832f),
	};
	constexpr int32 SlotCount = UE_ARRAY_COUNT(Slots);
	const int32 ActiveCount = FMath::Clamp(DiceCount, 1, SlotCount);
	FVector Centroid = FVector::ZeroVector;
	for (int32 SlotIndex = 0; SlotIndex < ActiveCount; ++SlotIndex)
	{
		Centroid += Slots[SlotIndex];
	}
	Centroid /= static_cast<float>(ActiveCount);
	const FVector LocalOffset = (Slots[Index % SlotCount] - Centroid) * (GatherSpacing * 0.78f);
	return BoardSurface->GetComponentTransform().TransformVectorNoScale(LocalOffset);
}

FVector AMouseGatherDiceManager::GetFloatingOffset(const FGatheredDie& State) const
{
	if (!IsValid(BoardSurface) || CarryFloatAmplitude <= 0.0f)
	{
		return FVector::ZeroVector;
	}
	const float Phase = State.CarryPhase;
	const FVector LocalOffset(
		FMath::Sin(CarryElapsed * 1.37f + Phase) * 0.42f,
		FMath::Cos(CarryElapsed * 1.11f + Phase * 1.23f) * 0.42f,
		FMath::Sin(CarryElapsed * 1.83f + Phase * 0.79f));
	return BoardSurface->GetComponentTransform().TransformVectorNoScale(LocalOffset * CarryFloatAmplitude);
}

void AMouseGatherDiceManager::UpdateNaturalDrop(const float DeltaSeconds)
{
	DropElapsed += DeltaSeconds;
	bool bAllSettled = DropElapsed >= 0.75f;
	for (FGatheredDie& State : GatheredDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (!IsValid(Body))
		{
			bAllSettled = false;
			continue;
		}
		if (DropElapsed >= 2.3f)
		{
			const float Alpha = FMath::Clamp((DropElapsed - 2.3f) / 1.4f, 0.0f, 1.0f);
			Body->SetLinearDamping(FMath::Lerp(0.04f, 1.4f, Alpha));
			Body->SetAngularDamping(FMath::Lerp(0.035f, 4.6f, Alpha));
		}
		bAllSettled = bAllSettled && State.bHadSupportContact
			&& Body->GetPhysicsLinearVelocity().Size() <= 10.0f
			&& Body->GetPhysicsAngularVelocityInRadians().Size() <= 0.55f;
	}
	StableElapsed = bAllSettled ? StableElapsed + DeltaSeconds : 0.0f;
	if (StableElapsed >= 0.30f || DropElapsed >= 7.0f)
	{
		FinishNaturalDrop();
	}
}

void AMouseGatherDiceManager::UpdatePredictedDrop()
{
	DropElapsed += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const bool bAnyRolling = GatheredDice.ContainsByPredicate([](const FGatheredDie& State)
	{
		const UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get();
		return IsValid(GuidedRoll) && GuidedRoll->IsRolling();
	});
	if (!bAnyRolling && DropElapsed >= 0.25f)
	{
		FinishPredictedDrop();
	}
}

void AMouseGatherDiceManager::HandleNaturalDropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (InteractionState != EMouseGatherState::NaturalDropping)
	{
		return;
	}
	if (FGatheredDie* State = GatheredDice.FindByPredicate([HitComponent](const FGatheredDie& Candidate)
		{
			return Candidate.Body.Get() == HitComponent;
		}))
	{
		const bool bHitBoard = OtherActor == BoardActor
			|| OtherComponent == BoardSurface
			|| (IsValid(OtherComponent) && OtherComponent->ComponentHasTag(TEXT("DiceBoundaryWall")));
		if (bHitBoard && DiceImpactFeedbackBridge::PlayMatchingGuidedImpact(
			State->GuidedRoll.Get(), HitComponent, NormalImpulse, Hit, State->LastImpactFeedbackTime))
		{
			++LastNaturalDropImpactFeedbackCount;
		}
		if (FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), GetBoardUpVector()) >= 0.5f)
		{
			State->bHadSupportContact = true;
		}
	}
}

void AMouseGatherDiceManager::FinishNaturalDrop()
{
	TArray<int32> Results;
	LastFinishedDice.Reset();
	LastFinishedDice.Reserve(GatheredDice.Num());
	for (FGatheredDie& State : GatheredDice)
	{
		UPrimitiveComponent* Body = State.Body.Get();
		if (IsValid(Body))
		{
			State.Result = ANaturalDiceRollManager::DetermineTopFace(
				Body->GetComponentQuat(), GetBoardUpVector());
		}
		Results.Add(State.Result);
		LastFinishedDice.Add(State.Dice.Get());
		SetGeneratedNumber(State.Dice.Get(), State.Result);
		RestoreDie(State, true);
		if (UDicePhysicsRollComponent* GuidedRoll = State.GuidedRoll.Get())
		{
			GuidedRoll->OnDiceRollFinished.Broadcast(State.Result);
		}
	}
	SetBoardBoundaryInset(0.0f, GatheredDice[0].Body.Get());
	GatheredDice.Reset();
	InteractionState = EMouseGatherState::Idle;
	SetActorTickEnabled(false);
	FString Values;
	for (const int32 Result : Results)
	{
		if (!Values.IsEmpty())
		{
			Values += TEXT(" · ");
		}
		Values += FString::FromInt(Result);
	}
	PublishStatus(FText::Format(NSLOCTEXT("MouseGatherDice", "NaturalResults",
		"Честное падение: {0}"), FText::FromString(Values)));
	OnFinished.Broadcast(Results);
	bUseTrajectoryGesture = false;
	bTrajectoryGestureActive = false;
}

void AMouseGatherDiceManager::FinishPredictedDrop()
{
	TArray<int32> Results;
	LastFinishedDice.Reset();
	LastFinishedDice.Reserve(GatheredDice.Num());
	for (FGatheredDie& State : GatheredDice)
	{
		Results.Add(State.Result);
		LastFinishedDice.Add(State.Dice.Get());
		RestoreGuidedSettings(State);
		RestoreDie(State, true);
	}
	SetBoardBoundaryInset(0.0f, GatheredDice[0].Body.Get());
	GatheredDice.Reset();
	InteractionState = EMouseGatherState::Idle;
	SetActorTickEnabled(false);
	FString Values;
	for (const int32 Result : Results)
	{
		if (!Values.IsEmpty())
		{
			Values += TEXT(" · ");
		}
		Values += FString::FromInt(Result);
	}
	PublishStatus(FText::Format(NSLOCTEXT("MouseGatherDice", "PredictedResults",
		"Падение с прогнозом: {0}"), FText::FromString(Values)));
	OnFinished.Broadcast(Results);
	bUseTrajectoryGesture = false;
	bTrajectoryGestureActive = false;
}

TArray<ACPP_Dice*> AMouseGatherDiceManager::GetLastFinishedDice() const
{
	TArray<ACPP_Dice*> Result;
	Result.Reserve(LastFinishedDice.Num());
	for (ACPP_Dice* Dice : LastFinishedDice)
	{
		Result.Add(Dice);
	}
	return Result;
}

void AMouseGatherDiceManager::RestoreGuidedSettings(FGatheredDie& State)
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

void AMouseGatherDiceManager::RestoreDie(FGatheredDie& State, const bool bKeepFinalTransform)
{
	UPrimitiveComponent* Body = State.Body.Get();
	ACPP_Dice* Dice = State.Dice.Get();
	if (IsValid(Body))
	{
		Body->OnComponentHit.RemoveDynamic(this, &AMouseGatherDiceManager::HandleNaturalDropHit);
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
			Body->AttachToComponent(State.OriginalAttachParent.Get(),
				FAttachmentTransformRules::KeepWorldTransform, State.OriginalAttachSocket);
		}
	}
	if (IsValid(Dice))
	{
		Dice->SetCanRollDice(State.bOriginalCanRoll);
	}
}

void AMouseGatherDiceManager::PublishStatus(const FText& Status)
{
	OnStatusChanged.Broadcast(Status);
}

void AMouseGatherDiceManager::SetGeneratedNumber(AActor* DiceActor, const int32 Result) const
{
	if (IsValid(DiceActor))
	{
		if (FIntProperty* Property = FindFProperty<FIntProperty>(DiceActor->GetClass(), TEXT("GeneratedNumber")))
		{
			Property->SetPropertyValue_InContainer(DiceActor, Result);
		}
	}
}

FRotator AMouseGatherDiceManager::GetLandingRotation(const int32 Result) const
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

AActor* AMouseGatherDiceManager::ResolveBoardActor()
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

UPrimitiveComponent* AMouseGatherDiceManager::ResolveBoardSurface()
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

FVector AMouseGatherDiceManager::GetBoardUpVector() const
{
	return IsValid(BoardSurface)
		? BoardSurface->GetComponentTransform().TransformVectorNoScale(FVector::UpVector).GetSafeNormal()
		: FVector::UpVector;
}

float AMouseGatherDiceManager::GetBoardClearance(const UPrimitiveComponent* Body) const
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
	const float Radius = FVector::DotProduct(Body->Bounds.BoxExtent.GetAbs(), GetBoardUpVector().GetAbs());
	return FVector::DotProduct(Body->GetComponentLocation() - BoardTop, GetBoardUpVector()) - Radius;
}

void AMouseGatherDiceManager::SetBoardBoundaryInset(const float EffectiveWallInset,
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

UBoxComponent* AMouseGatherDiceManager::CreateOrUpdateBoardCollider(const FName ColliderName,
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
