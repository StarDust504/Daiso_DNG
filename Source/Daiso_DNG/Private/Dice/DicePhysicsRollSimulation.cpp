// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DicePhysicsRollComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace DicePhysicsRoll
{
	constexpr float SmallNumber = 0.0001f;
}

// Обновляет физический полёт, плавно направляет нужную грань вверх и определяет момент завершения.
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
			const float FaceDot = FMath::Clamp(
				FVector::DotProduct(CurrentFaceNormal, FVector::UpVector), -1.0f, 1.0f);
			ErrorAngle = FMath::Acos(FaceDot);
			ErrorAxis = FVector::CrossProduct(CurrentFaceNormal, FVector::UpVector).GetSafeNormal();
			if (ErrorAxis.IsNearlyZero() && ErrorAngle > DicePhysicsRoll::SmallNumber)
			{
				ErrorAxis = FVector::CrossProduct(
					CurrentRotation.RotateVector(FVector::ForwardVector), FVector::UpVector).GetSafeNormal();
				if (ErrorAxis.IsNearlyZero())
				{
					ErrorAxis = FVector::RightVector;
				}
			}
			// Сохраняем заметное вращение по рысканию, гася только мешающее ориентации кувыркание.
			AngularVelocityToDamp -= FVector::UpVector
				* FVector::DotProduct(AngularVelocityToDamp, FVector::UpVector);
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
			const float TimeRampAlpha = FMath::Clamp(
				(RollElapsed - FreeFlightTime) / FMath::Max(AssistRampTime, 0.01f), 0.0f, 1.0f);
			const float ImpactWindowProgress = bHasImpactEstimate
				? FMath::Clamp(1.0f - EstimatedImpactTime / FMath::Max(AerialAlignmentLeadTime, 0.1f), 0.0f, 1.0f)
				: TimeRampAlpha;
			const float ImpactRampDurationFraction = FMath::Clamp(
				AssistRampTime / FMath::Max(AerialAlignmentLeadTime, 0.1f), 0.01f, 1.0f);
			const float RampAlpha = FMath::Clamp(
				ImpactWindowProgress / ImpactRampDurationFraction, 0.0f, 1.0f);
			const float StrengthScale = bHasMeaningfulImpact ? 1.0f : AerialStrengthMultiplier;
			const float DampingScale = bHasMeaningfulImpact ? 1.0f : AerialDampingMultiplier;
			const float AccelerationScale = bHasMeaningfulImpact ? 1.0f : AerialAccelerationMultiplier;
			const float EffectiveStrength = OrientationStrength * FMath::Lerp(0.15f, StrengthScale, RampAlpha);
			const float EffectiveDamping = OrientationDamping * FMath::Lerp(0.15f, DampingScale, RampAlpha);
			const float EffectiveMaxAcceleration = MaxAngularAcceleration
				* FMath::Lerp(0.25f, AccelerationScale, RampAlpha);
			FVector AngularAcceleration = ErrorAxis.GetSafeNormal() * (ErrorAngle * EffectiveStrength)
				- AngularVelocityToDamp * EffectiveDamping;
			AngularAcceleration = AngularAcceleration.GetClampedToMaxSize(EffectiveMaxAcceleration);
			ActiveBody->AddTorqueInRadians(AngularAcceleration, NAME_None, true);
		}
	}

	// В последние мгновения полёта распределяем оставшуюся ошибку до контакта со столом.
	// После настоящего приземления этот путь больше не меняет поворот кубика.
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
			const FQuat NewRotation = FQuat::Slerp(
				CurrentRotation, AirborneTarget, AlignmentAlpha).GetNormalized();
			ActiveBody->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

			// Оставляем небольшую визуальную составляющую рыскания, но убираем опасное кувыркание.
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
			// После контакта сохраняем поворот, полученный физикой.
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
			// Тайм-аут не имеет права поворачивать уже коснувшийся стола кубик.
			CompleteRoll(false);
		}
		else
		{
			BeginFinalAlignment();
		}
	}
}

// Останавливает физику и начинает короткую визуально плавную доводку поворота.
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

// Интерполирует кубик к целевому повороту во время финального выравнивания.
void UDicePhysicsRollComponent::UpdateFinalAlignment(const float DeltaTime)
{
	if (!IsValid(ActiveBody))
	{
		CancelRoll(false);
		return;
	}

	FinalAlignmentElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(
		FinalAlignmentElapsed / FMath::Max(FinalAlignmentTime, DicePhysicsRoll::SmallNumber), 0.0f, 1.0f);
	// Плавное замедление делает последние градусы коррекции незаметными.
	const float SmoothedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
	const FQuat NewRotation = FQuat::Slerp(
		FinalAlignmentStartRotation, TargetWorldRotation, SmoothedAlpha).GetNormalized();
	ActiveBody->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		CompleteRoll(true);
	}
}

// Завершает бросок, при необходимости применяет цель и рассылает итоговое значение подписчикам.
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

// Обрабатывает физический удар, распознаёт приземление и при необходимости запускает корректирующий отскок.
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

			// Сохраняем горизонтальное движение и добавляем только импульс для небольшого отскока.
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
			// После этого контакта ориентационная помощь больше не меняет поворот кубика.
			ActiveBody->SetLinearDamping(LandingLinearDamping);
			ActiveBody->SetAngularDamping(LandingAngularDamping);
		}
	}
	else if (bLaunchContact)
	{
		// Chaos может прислать старый контакт стартовавшего со стола кубика; это ещё не приземление.
		return;
	}

	const float SafeMass = FMath::Max(ActiveBody->GetMass(), 0.001f);
	const float ImpulseSpeed = NormalImpulse.Size() / SafeMass;
	const float VelocityIntoSurface = FMath::Abs(
		FVector::DotProduct(ActiveBody->GetPhysicsLinearVelocity(), Hit.ImpactNormal));
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

// Возвращает угловую ошибку между текущим и полным целевым поворотом кубика.
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

// Возвращает угловое отклонение выбранной грани от направления вверх.
float UDicePhysicsRollComponent::GetFaceUpErrorRadians() const
{
	if (!IsValid(ActiveBody) || !bHasFaceTarget)
	{
		return PI;
	}

	const FVector CurrentFaceNormal = ActiveBody->GetComponentQuat()
		.RotateVector(ActiveFaceNormalLocal).GetSafeNormal();
	const float Dot = FMath::Clamp(
		FVector::DotProduct(CurrentFaceNormal, FVector::UpVector), -1.0f, 1.0f);
	return FMath::Acos(Dot);
}

// Оценивает оставшееся время полёта до верхней плоскости игрового стола.
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
	return (UpwardVelocity + FMath::Sqrt(
		FMath::Square(UpwardVelocity) + 2.0f * DownwardAcceleration * Height)) / DownwardAcceleration;
}
