// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DicePhysicsRollComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace DicePhysicsRoll
{
	constexpr float SmallNumber = 0.0001f;
}

UDicePhysicsRollComponent::UDicePhysicsRollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Conventional right-handed layout. Calibrate these six vectors once if the imported mesh uses another orientation.
	FaceLocalNormals.Add(1, FVector::UpVector);
	FaceLocalNormals.Add(2, FVector::ForwardVector);
	FaceLocalNormals.Add(3, FVector::RightVector);
	FaceLocalNormals.Add(4, FVector::LeftVector);
	FaceLocalNormals.Add(5, FVector::BackwardVector);
	FaceLocalNormals.Add(6, FVector::DownVector);
}

void UDicePhysicsRollComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindDiceBody)
	{
		ResolveDiceBody();
	}
}

void UDicePhysicsRollComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelRoll(false);
	Super::EndPlay(EndPlayReason);
}

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
	const float LandingYawRadians = bRandomizeLandingYaw ? FMath::DegreesToRadians(FMath::FRandRange(0.0f, 360.0f)) : 0.0f;
	const FQuat YawRotation(FVector::UpVector, LandingYawRadians);
	ActiveFaceNormalLocal = NormalizedFace;
	bHasFaceTarget = true;
	bLandingYawResolved = !bPreserveNaturalLandingYaw;
	return StartRoll(Result, (YawRotation * FaceToUp).GetNormalized());
}

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

void UDicePhysicsRollComponent::UpdateSimulation(const float DeltaTime)
{
	if (!IsValid(ActiveBody))
	{
		CancelRoll(false);
		return;
	}

	RollElapsed += DeltaTime;
	const FVector LinearVelocity = ActiveBody->GetPhysicsLinearVelocity();
	const FVector AngularVelocity = ActiveBody->GetPhysicsAngularVelocityInRadians();
	if (GetBoardClearance() >= MinimumAirborneClearance)
	{
		bHasBeenAirborne = true;
	}
	const bool bFreeFlightFinished = RollElapsed >= FreeFlightTime;
	const float EstimatedImpactTime = GetEstimatedTimeToBoardImpact(LinearVelocity);
	const bool bHasImpactEstimate = EstimatedImpactTime >= 0.0f;
	const bool bInsideAerialAlignmentWindow = !bHasImpactEstimate
		|| EstimatedImpactTime <= AerialAlignmentLeadTime;
	const bool bMayAssist = bFreeFlightFinished
		&& bInsideAerialAlignmentWindow
		&& (!bAssistOnlyWhileFalling || LinearVelocity.Z <= 0.0f)
		&& (bAllowPostImpactOrientationAssist || !bHasBoardImpact);

	if (bMayAssist)
	{
		const FQuat CurrentRotation = ActiveBody->GetComponentQuat();
		if (bHasFaceTarget && !bLandingYawResolved)
		{
			TargetWorldRotation = ResolveNaturalLandingYaw(ActiveFaceNormalLocal, CurrentRotation);
			bLandingYawResolved = true;
		}

		FVector ErrorAxis = FVector::UpVector;
		float ErrorAngle = 0.0f;
		FVector AngularVelocityToDamp = AngularVelocity;
		if (bHasFaceTarget && bPreserveNaturalLandingYaw)
		{
			const FVector CurrentFaceNormal = CurrentRotation.RotateVector(ActiveFaceNormalLocal).GetSafeNormal();
			const float FaceDot = FMath::Clamp(FVector::DotProduct(CurrentFaceNormal, FVector::UpVector), -1.0f, 1.0f);
			ErrorAngle = FMath::Acos(FaceDot);
			ErrorAxis = FVector::CrossProduct(CurrentFaceNormal, FVector::UpVector).GetSafeNormal();
			if (ErrorAxis.IsNearlyZero() && ErrorAngle > DicePhysicsRoll::SmallNumber)
			{
				ErrorAxis = FVector::CrossProduct(CurrentRotation.RotateVector(FVector::ForwardVector), FVector::UpVector).GetSafeNormal();
				if (ErrorAxis.IsNearlyZero())
				{
					ErrorAxis = FVector::RightVector;
				}
			}
			// Preserve visible yaw spin in the air; only the tumbling that prevents the face from pointing up is damped.
			AngularVelocityToDamp -= FVector::UpVector * FVector::DotProduct(AngularVelocityToDamp, FVector::UpVector);
		}
		else
		{
			FQuat ErrorRotation = (TargetWorldRotation * CurrentRotation.Inverse()).GetNormalized();
			if (ErrorRotation.W < 0.0f)
			{
				ErrorRotation.X *= -1.0f;
				ErrorRotation.Y *= -1.0f;
				ErrorRotation.Z *= -1.0f;
				ErrorRotation.W *= -1.0f;
			}
			ErrorRotation.ToAxisAndAngle(ErrorAxis, ErrorAngle);
		}

		if (!ErrorAxis.ContainsNaN() && ErrorAngle > DicePhysicsRoll::SmallNumber)
		{
			const float TimeRampAlpha = FMath::Clamp((RollElapsed - FreeFlightTime) / FMath::Max(AssistRampTime, 0.01f), 0.0f, 1.0f);
			const float ImpactWindowProgress = bHasImpactEstimate
				? FMath::Clamp(1.0f - EstimatedImpactTime / FMath::Max(AerialAlignmentLeadTime, 0.1f), 0.0f, 1.0f)
				: TimeRampAlpha;
			const float ImpactRampDurationFraction = FMath::Clamp(AssistRampTime / FMath::Max(AerialAlignmentLeadTime, 0.1f), 0.01f, 1.0f);
			const float RampAlpha = FMath::Clamp(ImpactWindowProgress / ImpactRampDurationFraction, 0.0f, 1.0f);
			const float StrengthScale = bHasMeaningfulImpact ? 1.0f : AerialStrengthMultiplier;
			const float DampingScale = bHasMeaningfulImpact ? 1.0f : AerialDampingMultiplier;
			const float AccelerationScale = bHasMeaningfulImpact ? 1.0f : AerialAccelerationMultiplier;
			const float EffectiveStrength = OrientationStrength * FMath::Lerp(0.15f, StrengthScale, RampAlpha);
			const float EffectiveDamping = OrientationDamping * FMath::Lerp(0.15f, DampingScale, RampAlpha);
			const float EffectiveMaxAcceleration = MaxAngularAcceleration * FMath::Lerp(0.25f, AccelerationScale, RampAlpha);
			FVector AngularAcceleration = ErrorAxis.GetSafeNormal() * (ErrorAngle * EffectiveStrength)
				- AngularVelocityToDamp * EffectiveDamping;
			AngularAcceleration = AngularAcceleration.GetClampedToMaxSize(EffectiveMaxAcceleration);
			ActiveBody->AddTorqueInRadians(AngularAcceleration, NAME_None, true);
		}
	}

	// The spring above normally gets the requested face almost all the way there. In the last fraction of
	// airborne time, distribute any remaining error over the frames left before contact. This is deliberately
	// performed in the air: once a genuine landing hit is received, no rotation path below is allowed to run.
	const bool bMayUseAirborneSafety = bUseAirborneSafetyAlignment
		&& bHasFaceTarget
		&& bHasBeenAirborne
		&& !bHasBoardImpact
		&& bHasImpactEstimate
		&& EstimatedImpactTime <= AirborneSafetyAlignmentTime
		&& GetBoardClearance() > 0.0f;
	if (bMayUseAirborneSafety)
	{
		const float FaceError = GetFaceUpErrorRadians();
		if (FaceError > FMath::DegreesToRadians(AirborneSafetyTolerance))
		{
			const FQuat CurrentRotation = ActiveBody->GetComponentQuat();
			const FQuat AirborneTarget = bPreserveNaturalLandingYaw
				? ResolveNaturalLandingYaw(ActiveFaceNormalLocal, CurrentRotation)
				: TargetWorldRotation;
			const float RemainingTime = FMath::Max(EstimatedImpactTime, DeltaTime);
			const float AlignmentAlpha = FMath::Clamp(DeltaTime / RemainingTime, 0.0f, 1.0f);
			const FQuat NewRotation = FQuat::Slerp(CurrentRotation, AirborneTarget, AlignmentAlpha).GetNormalized();
			ActiveBody->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

			// Keep a little of the visually useful yaw, but remove tumbling that could undo the captured face
			// during the final centimetres of the fall.
			const FVector BoardUp = GetBoardUpVector();
			const FVector RetainedYawSpin = BoardUp
				* FVector::DotProduct(AngularVelocity, BoardUp)
				* AirborneYawSpinRetention;
			ActiveBody->SetPhysicsAngularVelocityInRadians(RetainedYawSpin, false);
		}
	}

	const float OrientationError = bHasFaceTarget ? GetFaceUpErrorRadians() : GetOrientationErrorRadians();
	const bool bSlowEnough = LinearVelocity.Size() <= SettleLinearSpeed
		&& AngularVelocity.Size() <= SettleAngularSpeed;
	const bool bAlignedEnough = OrientationError <= FMath::DegreesToRadians(SettleAngleTolerance);

	if (bFreeFlightFinished && bSlowEnough && bAlignedEnough)
	{
		StableElapsed += DeltaTime;
	}
	else
	{
		StableElapsed = 0.0f;
	}

	if (StableElapsed >= RequiredStableTime)
	{
		if (bHasBoardImpact)
		{
			// Board contact is irreversible: keep exactly the orientation produced by physics.
			CompleteRoll(false);
		}
		else
		{
			BeginFinalAlignment();
		}
	}
	else if (RollElapsed >= MaximumRollTime)
	{
		if (bHasBoardImpact)
		{
			// Never use the timeout fallback to turn a die that has already touched the board.
			CompleteRoll(false);
		}
		else
		{
			BeginFinalAlignment();
		}
	}
}

void UDicePhysicsRollComponent::BeginFinalAlignment()
{
	if (!IsValid(ActiveBody))
	{
		CancelRoll(false);
		return;
	}
	if (bHasBoardImpact)
	{
		CompleteRoll(false);
		return;
	}

	FinalAlignmentStartRotation = ActiveBody->GetComponentQuat();
	FinalAlignmentElapsed = 0.0f;
	ActiveBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
	ActiveBody->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	ActiveBody->SetSimulatePhysics(false);
	RollState = ERollState::FinalAlign;

	if (FinalAlignmentTime <= DicePhysicsRoll::SmallNumber)
	{
		CompleteRoll(true);
	}
}

void UDicePhysicsRollComponent::UpdateFinalAlignment(const float DeltaTime)
{
	if (!IsValid(ActiveBody))
	{
		CancelRoll(false);
		return;
	}

	FinalAlignmentElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(FinalAlignmentElapsed / FMath::Max(FinalAlignmentTime, DicePhysicsRoll::SmallNumber), 0.0f, 1.0f);
	// Ease out: most of the tiny correction happens immediately and the last few degrees settle softly.
	const float SmoothedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
	const FQuat NewRotation = FQuat::Slerp(FinalAlignmentStartRotation, TargetWorldRotation, SmoothedAlpha).GetNormalized();
	ActiveBody->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		CompleteRoll(true);
	}
}

void UDicePhysicsRollComponent::CompleteRoll(const bool bApplyTargetRotation)
{
	if (!IsValid(ActiveBody))
	{
		CancelRoll(false);
		return;
	}

	if (bApplyTargetRotation)
	{
		ActiveBody->SetWorldRotation(TargetWorldRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	RestoreBodySettings();

	if (bFreezeAfterLanding)
	{
		ActiveBody->SetSimulatePhysics(false);
		if (OriginalAttachParent.IsValid())
		{
			ActiveBody->AttachToComponent(OriginalAttachParent.Get(),
				FAttachmentTransformRules::KeepWorldTransform, OriginalAttachSocket);
		}
	}
	else
	{
		ActiveBody->SetSimulatePhysics(true);
		ActiveBody->WakeAllRigidBodies();
	}

	const int32 FinishedResult = ActiveResult;
	RollState = ERollState::Idle;
	SetComponentTickEnabled(false);
	OnDiceRollFinished.Broadcast(FinishedResult);
}

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

bool UDicePhysicsRollComponent::IsRolling() const
{
	return RollState != ERollState::Idle;
}

void UDicePhysicsRollComponent::HandleDiceHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, const FVector NormalImpulse, const FHitResult& Hit)
{
	if (RollState != ERollState::Simulating || HitComponent != ActiveBody)
	{
		return;
	}

	const bool bHitBoard = OtherActor == ActiveBoardActor
		|| OtherComponent == ActiveBoardSurface
		|| (IsValid(OtherComponent) && OtherComponent->ComponentHasTag(TEXT("DiceBoundaryWall")));
	const FVector BoardUp = GetBoardUpVector();
	const bool bHitUpwardFacingSupport = FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), BoardUp)
		>= LandingSurfaceNormalDot;
	const bool bHitAnotherDynamicBody = IsValid(OtherComponent) && OtherComponent->IsSimulatingPhysics();
	const bool bGenuineLanding = bHasBeenAirborne
		&& bHitUpwardFacingSupport
		&& (bHitBoard || bHitAnotherDynamicBody);
	const bool bLaunchContact = bHitBoard && bHitUpwardFacingSupport && !bHasBeenAirborne;
	bool bStartedCorrectionBounce = false;
	if (bGenuineLanding)
	{
		const float FaceError = bHasFaceTarget ? GetFaceUpErrorRadians() : 0.0f;
		const bool bWrongFace = FaceError > FMath::DegreesToRadians(CorrectionBounceTriggerAngle);
		if (bRetryWrongFaceWithBounce && bWrongFace && CorrectionBounceCount < MaximumCorrectionBounces)
		{
			++CorrectionBounceCount;
			bStartedCorrectionBounce = true;
			bHasBoardImpact = false;
			bHasBeenAirborne = false;
			bHasMeaningfulImpact = false;
			ActiveBody->SetLinearDamping(AirLinearDamping);
			ActiveBody->SetAngularDamping(AirAngularDamping);

			// Preserve the horizontal motion from the collision and add only the velocity needed to reach
			// the configured low hop. The small tangent kick stops retries from looking mechanically vertical.
			const FVector CurrentVelocity = ActiveBody->GetPhysicsLinearVelocity();
			const float CurrentUpwardSpeed = FVector::DotProduct(CurrentVelocity, BoardUp);
			const float RequiredUpwardKick = FMath::Max(CorrectionBounceSpeed - CurrentUpwardSpeed, 0.0f);
			FVector RandomTangent = FMath::VRand();
			RandomTangent -= BoardUp * FVector::DotProduct(RandomTangent, BoardUp);
			RandomTangent = RandomTangent.GetSafeNormal();
			ActiveBody->AddImpulse(BoardUp * RequiredUpwardKick
				+ RandomTangent * CorrectionBounceHorizontalSpeed, NAME_None, true);

			const FQuat CurrentRotation = ActiveBody->GetComponentQuat();
			const FVector CurrentFaceNormal = CurrentRotation.RotateVector(ActiveFaceNormalLocal).GetSafeNormal();
			FVector CorrectionAxis = FVector::CrossProduct(CurrentFaceNormal, BoardUp).GetSafeNormal();
			if (CorrectionAxis.IsNearlyZero() && FaceError > HALF_PI)
			{
				CorrectionAxis = FVector::CrossProduct(
					CurrentRotation.RotateVector(FVector::ForwardVector), BoardUp).GetSafeNormal();
			}
			if (!CorrectionAxis.IsNearlyZero())
			{
				ActiveBody->AddAngularImpulseInRadians(
					CorrectionAxis * CorrectionBounceAngularSpeed, NAME_None, true);
			}
		}
		else
		{
			bHasBoardImpact = true;
			// From this callback onward no code path may add orientation torque or apply a target rotation.
			// Requiring an airborne phase prevents the launch-frame contact with the board from disabling guidance.
			ActiveBody->SetLinearDamping(LandingLinearDamping);
			ActiveBody->SetAngularDamping(LandingAngularDamping);
		}
	}
	else if (bLaunchContact)
	{
		// A die may begin the throw resting on the board. Chaos can report that old contact after the
		// launch impulse; it is neither a landing nor an impact and must not switch to landing damping.
		return;
	}

	const float SafeMass = FMath::Max(ActiveBody->GetMass(), 0.001f);
	const float ImpulseSpeed = NormalImpulse.Size() / SafeMass;
	const float VelocityIntoSurface = FMath::Abs(FVector::DotProduct(ActiveBody->GetPhysicsLinearVelocity(), Hit.ImpactNormal));
	const float ImpactSpeed = FMath::Max(ImpulseSpeed, VelocityIntoSurface);
	if (ImpactSpeed < MinimumImpactSpeed)
	{
		return;
	}

	if (!bStartedCorrectionBounce && !bHasMeaningfulImpact)
	{
		bHasMeaningfulImpact = true;
		ActiveBody->SetLinearDamping(LandingLinearDamping);
		ActiveBody->SetAngularDamping(LandingAngularDamping);
	}

	const float Denominator = FMath::Max(FullStrengthImpactSpeed - MinimumImpactSpeed, 1.0f);
	const float Strength = FMath::Clamp((ImpactSpeed - MinimumImpactSpeed) / Denominator, 0.0f, 1.0f);
	OnDiceImpact.Broadcast(Strength, Hit.ImpactPoint);
}

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

AActor* UDicePhysicsRollComponent::ResolveBoardActor()
{
	if (IsValid(BoardActorOverride))
	{
		ActiveBoardActor = BoardActorOverride;
		return ActiveBoardActor;
	}

	if (IsValid(ActiveBoardActor) || !bAutoFindBoard || !GetWorld())
	{
		return ActiveBoardActor;
	}

	AActor* NameHintCandidate = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == GetOwner())
		{
			continue;
		}

		if (!BoardActorTag.IsNone() && Candidate->ActorHasTag(BoardActorTag))
		{
			ActiveBoardActor = Candidate;
			return ActiveBoardActor;
		}

		if (!BoardClassNameHint.IsEmpty() && Candidate->GetClass()->GetName().Contains(BoardClassNameHint))
		{
			NameHintCandidate = Candidate;
		}
	}

	ActiveBoardActor = NameHintCandidate;
	return ActiveBoardActor;
}

UPrimitiveComponent* UDicePhysicsRollComponent::ResolveBoardSurface()
{
	if (IsValid(ActiveBoardSurface))
	{
		return ActiveBoardSurface;
	}

	AActor* Board = ResolveBoardActor();
	if (!IsValid(Board))
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Board->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	float LargestHorizontalArea = -1.0f;
	for (UPrimitiveComponent* Candidate : PrimitiveComponents)
	{
		if (!IsValid(Candidate) || Candidate->ComponentHasTag(TEXT("DiceBoundaryWall")))
		{
			continue;
		}

		const FBoxSphereBounds LocalBounds = Candidate->CalcBounds(FTransform::Identity);
		const float HorizontalArea = LocalBounds.BoxExtent.X * LocalBounds.BoxExtent.Y;
		if (HorizontalArea > LargestHorizontalArea)
		{
			LargestHorizontalArea = HorizontalArea;
			ActiveBoardSurface = Candidate;
		}
	}

	return ActiveBoardSurface;
}

void UDicePhysicsRollComponent::EnsureBoardBoundaryWalls()
{
	if (!bCreateBoardBoundaryWalls || !IsValid(ActiveBody))
	{
		return;
	}

	UPrimitiveComponent* BoardSurface = ResolveBoardSurface();
	if (!IsValid(BoardSurface) || !IsValid(ActiveBoardActor))
	{
		return;
	}

	TArray<UActorComponent*> ExistingColliders = ActiveBoardActor->GetComponentsByTag(UBoxComponent::StaticClass(), TEXT("DiceBoundaryWall"));
	if (ExistingColliders.Num() >= 5)
	{
		return;
	}

	const FBoxSphereBounds LocalBounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector LocalMin = LocalBounds.Origin - LocalBounds.BoxExtent;
	const FVector LocalMax = LocalBounds.Origin + LocalBounds.BoxExtent;
	const FVector AbsScale = BoardSurface->GetComponentTransform().GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
	const float HalfThicknessX = BoardWallThickness * 0.5f / AbsScale.X;
	const float HalfThicknessY = BoardWallThickness * 0.5f / AbsScale.Y;
	const float HalfFloorThickness = BoardFloorThickness * 0.5f / AbsScale.Z;
	const float HalfHeight = BoardWallHeight * 0.5f / AbsScale.Z;
	const float InsetX = BoardWallInset / AbsScale.X;
	const float InsetY = BoardWallInset / AbsScale.Y;
	const float TopZ = LocalMax.Z;
	const float WallZ = TopZ + HalfHeight;

	const float LeftX = LocalMin.X + InsetX - HalfThicknessX;
	const float RightX = LocalMax.X - InsetX + HalfThicknessX;
	const float BottomY = LocalMin.Y + InsetY - HalfThicknessY;
	const float TopY = LocalMax.Y - InsetY + HalfThicknessY;
	const float BoardHalfX = FMath::Max((LocalMax.X - LocalMin.X) * 0.5f, 1.0f);
	const float BoardHalfY = FMath::Max((LocalMax.Y - LocalMin.Y) * 0.5f, 1.0f);

	// A dedicated simple floor is required because thin/complex board mesh collision can be missed by Chaos.
	CreateBoardCollider(TEXT("DiceBoundary_Floor"),
		FVector(LocalBounds.Origin.X, LocalBounds.Origin.Y, TopZ - HalfFloorThickness),
		FVector(BoardHalfX, BoardHalfY, HalfFloorThickness));
	CreateBoardCollider(TEXT("DiceBoundary_XMin"), FVector(LeftX, LocalBounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight));
	CreateBoardCollider(TEXT("DiceBoundary_XMax"), FVector(RightX, LocalBounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight));
	CreateBoardCollider(TEXT("DiceBoundary_YMin"), FVector(LocalBounds.Origin.X, BottomY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight));
	CreateBoardCollider(TEXT("DiceBoundary_YMax"), FVector(LocalBounds.Origin.X, TopY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight));
}

UBoxComponent* UDicePhysicsRollComponent::CreateBoardCollider(const FName ColliderName, const FVector& RelativeLocation,
	const FVector& LocalBoxExtent)
{
	if (!IsValid(ActiveBoardActor) || !IsValid(ActiveBoardSurface))
	{
		return nullptr;
	}

	if (UBoxComponent* ExistingCollider = FindObject<UBoxComponent>(ActiveBoardActor, *ColliderName.ToString()))
	{
		return ExistingCollider;
	}

	UBoxComponent* Wall = NewObject<UBoxComponent>(ActiveBoardActor, ColliderName);
	ActiveBoardActor->AddInstanceComponent(Wall);
	Wall->ComponentTags.AddUnique(TEXT("DiceBoundaryWall"));
	Wall->SetupAttachment(ActiveBoardSurface);
	Wall->SetRelativeLocation(RelativeLocation);
	Wall->SetRelativeRotation(FRotator::ZeroRotator);
	Wall->SetBoxExtent(LocalBoxExtent, false);
	Wall->SetMobility(ActiveBoardSurface->Mobility);
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
	Wall->SetCollisionResponseToChannel(ActiveBody->GetCollisionObjectType(), ECR_Block);
	Wall->SetGenerateOverlapEvents(false);
	Wall->SetCanEverAffectNavigation(false);
	Wall->SetHiddenInGame(true);
	Wall->RegisterComponent();
	return Wall;
}

FVector UDicePhysicsRollComponent::GetBoardAwareHorizontalVelocity() const
{
	const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	FVector RandomDirection(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.0f);
	if (!IsValid(ActiveBoardSurface) || !IsValid(ActiveBody))
	{
		return RandomDirection * HorizontalSpeed;
	}

	const FTransform BoardTransform = ActiveBoardSurface->GetComponentTransform();
	const FBoxSphereBounds LocalBounds = ActiveBoardSurface->CalcBounds(FTransform::Identity);
	const FVector LocalPosition = BoardTransform.InverseTransformPosition(ActiveBody->GetComponentLocation());
	const FVector LocalOffset = LocalPosition - LocalBounds.Origin;

	if (bClusterThrowsOnBoard)
	{
		const float ClusterAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float ClusterRadius = FMath::Sqrt(FMath::FRand()) * BoardLandingClusterRadius;
		FVector TargetLocal = LocalBounds.Origin;
		TargetLocal.X += FMath::Cos(ClusterAngle) * LocalBounds.BoxExtent.X * ClusterRadius;
		TargetLocal.Y += FMath::Sin(ClusterAngle) * LocalBounds.BoxExtent.Y * ClusterRadius;
		TargetLocal.Z = LocalBounds.Origin.Z + LocalBounds.BoxExtent.Z;

		const FVector TargetWorld = BoardTransform.TransformPosition(TargetLocal);
		FVector ToTarget = TargetWorld - ActiveBody->GetComponentLocation();
		ToTarget.Z = 0.0f;
		const float GravityMagnitude = FMath::Max(FMath::Abs(GetWorld() ? GetWorld()->GetGravityZ() : -980.0f), 1.0f);
		const float HeightAboveBoard = FMath::Max(ActiveBody->GetComponentLocation().Z - TargetWorld.Z, 0.0f);
		const float ExpectedAirTime = (UpwardSpeed + FMath::Sqrt(FMath::Square(UpwardSpeed) + 2.0f * GravityMagnitude * HeightAboveBoard))
			/ GravityMagnitude;
		return (ToTarget / FMath::Max(ExpectedAirTime, 0.20f)).GetClampedToMaxSize(HorizontalSpeed);
	}

	if (!bBiasThrowTowardBoardCenter)
	{
		return RandomDirection * HorizontalSpeed;
	}

	const float NormalizedX = FMath::Abs(LocalOffset.X) / FMath::Max(LocalBounds.BoxExtent.X, 1.0f);
	const float NormalizedY = FMath::Abs(LocalOffset.Y) / FMath::Max(LocalBounds.BoxExtent.Y, 1.0f);
	const float EdgeAmount = FMath::GetRangePct(BoardEdgeBiasStart, 1.0f, FMath::Max(NormalizedX, NormalizedY));
	const float Bias = FMath::Clamp(FMath::Lerp(BoardCenterBias, BoardEdgeBias, FMath::Clamp(EdgeAmount, 0.0f, 1.0f)), 0.0f, 1.0f);

	FVector ToCenter = BoardTransform.TransformVectorNoScale(FVector(-LocalOffset.X, -LocalOffset.Y, 0.0f));
	ToCenter.Z = 0.0f;
	ToCenter = ToCenter.GetSafeNormal();
	if (ToCenter.IsNearlyZero())
	{
		return RandomDirection * HorizontalSpeed;
	}

	RandomDirection = FMath::Lerp(RandomDirection, ToCenter, Bias).GetSafeNormal();
	return (RandomDirection.IsNearlyZero() ? ToCenter : RandomDirection) * HorizontalSpeed;
}

FQuat UDicePhysicsRollComponent::ResolveNaturalLandingYaw(const FVector& FaceNormalLocal, const FQuat& CurrentRotation) const
{
	const FVector Normal = FaceNormalLocal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return TargetWorldRotation;
	}

	const FQuat FaceToUp = FQuat::FindBetweenNormals(Normal, FVector::UpVector);
	const FVector ReferenceAxis = FMath::Abs(FVector::DotProduct(Normal, FVector::ForwardVector)) < 0.9f
		? FVector::ForwardVector
		: FVector::RightVector;
	const FVector LocalTangent = (ReferenceAxis - Normal * FVector::DotProduct(ReferenceAxis, Normal)).GetSafeNormal();
	FVector BaseTangent = FaceToUp.RotateVector(LocalTangent);
	FVector CurrentTangent = CurrentRotation.RotateVector(LocalTangent);
	BaseTangent.Z = 0.0f;
	CurrentTangent.Z = 0.0f;
	BaseTangent = BaseTangent.GetSafeNormal();
	CurrentTangent = CurrentTangent.GetSafeNormal();
	if (BaseTangent.IsNearlyZero() || CurrentTangent.IsNearlyZero())
	{
		return FaceToUp;
	}

	const float YawAngle = FMath::Atan2(FVector::CrossProduct(BaseTangent, CurrentTangent).Z,
		FVector::DotProduct(BaseTangent, CurrentTangent));
	return (FQuat(FVector::UpVector, YawAngle) * FaceToUp).GetNormalized();
}

float UDicePhysicsRollComponent::GetOrientationErrorRadians() const
{
	if (!IsValid(ActiveBody))
	{
		return PI;
	}

	const FQuat CurrentRotation = ActiveBody->GetComponentQuat();
	const double Dot = CurrentRotation.X * TargetWorldRotation.X
		+ CurrentRotation.Y * TargetWorldRotation.Y
		+ CurrentRotation.Z * TargetWorldRotation.Z
		+ CurrentRotation.W * TargetWorldRotation.W;
	const double AbsDot = FMath::Clamp(FMath::Abs(Dot), 0.0, 1.0);
	return 2.0f * FMath::Acos(AbsDot);
}

float UDicePhysicsRollComponent::GetFaceUpErrorRadians() const
{
	if (!IsValid(ActiveBody) || !bHasFaceTarget)
	{
		return PI;
	}

	const FVector CurrentFaceNormal = ActiveBody->GetComponentQuat().RotateVector(ActiveFaceNormalLocal).GetSafeNormal();
	const float Dot = FMath::Clamp(FVector::DotProduct(CurrentFaceNormal, FVector::UpVector), -1.0f, 1.0f);
	return FMath::Acos(Dot);
}

float UDicePhysicsRollComponent::GetEstimatedTimeToBoardImpact(const FVector& LinearVelocity) const
{
	if (!IsValid(ActiveBody) || !IsValid(ActiveBoardSurface))
	{
		return -1.0f;
	}

	const FVector BoardUp = GetBoardUpVector();
	const float Height = FMath::Max(GetBoardClearance(), 0.0f);
	const float UpwardVelocity = FVector::DotProduct(LinearVelocity, BoardUp);
	const FVector Gravity = FVector(0.0f, 0.0f, GetWorld() ? GetWorld()->GetGravityZ() : -980.0f);
	const float DownwardAcceleration = FMath::Max(-FVector::DotProduct(Gravity, BoardUp), 1.0f);
	return (UpwardVelocity + FMath::Sqrt(FMath::Square(UpwardVelocity) + 2.0f * DownwardAcceleration * Height))
		/ DownwardAcceleration;
}

float UDicePhysicsRollComponent::GetBoardClearance() const
{
	if (!IsValid(ActiveBody) || !IsValid(ActiveBoardSurface))
	{
		return -1.0f;
	}

	const FTransform BoardTransform = ActiveBoardSurface->GetComponentTransform();
	const FBoxSphereBounds LocalBounds = ActiveBoardSurface->CalcBounds(FTransform::Identity);
	const FVector BoardTop = BoardTransform.TransformPosition(
		FVector(LocalBounds.Origin.X, LocalBounds.Origin.Y, LocalBounds.Origin.Z + LocalBounds.BoxExtent.Z));
	const FVector BoardUp = GetBoardUpVector();
	const float BodySupportRadius = FVector::DotProduct(ActiveBody->Bounds.BoxExtent.GetAbs(), BoardUp.GetAbs());
	return FVector::DotProduct(ActiveBody->GetComponentLocation() - BoardTop, BoardUp) - BodySupportRadius;
}

FVector UDicePhysicsRollComponent::GetBoardUpVector() const
{
	return IsValid(ActiveBoardSurface)
		? ActiveBoardSurface->GetComponentTransform().TransformVectorNoScale(FVector::UpVector).GetSafeNormal()
		: FVector::UpVector;
}

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
