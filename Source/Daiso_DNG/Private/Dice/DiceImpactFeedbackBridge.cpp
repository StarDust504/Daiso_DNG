// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DiceImpactFeedbackBridge.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

bool DiceImpactFeedbackBridge::PlayMatchingGuidedImpact(
	UDicePhysicsRollComponent* FeedbackSource,
	UPrimitiveComponent* HitBody,
	const FVector& NormalImpulse,
	const FHitResult& Hit,
	double& InOutLastFeedbackTime)
{
	if (!IsValid(FeedbackSource) || !IsValid(HitBody) || !IsValid(FeedbackSource->GetWorld()))
	{
		return false;
	}

	const float SafeMass = FMath::Max(HitBody->GetMass(), 0.001f);
	const float ImpulseSpeed = NormalImpulse.Size() / SafeMass;
	const float VelocityIntoSurface = FMath::Abs(FVector::DotProduct(
		HitBody->GetPhysicsLinearVelocity(), Hit.ImpactNormal.GetSafeNormal()));
	const float ImpactSpeed = FMath::Max(ImpulseSpeed, VelocityIntoSurface);
	if (ImpactSpeed < FeedbackSource->MinimumImpactSpeed)
	{
		return false;
	}

	const float Denominator = FMath::Max(
		FeedbackSource->FullStrengthImpactSpeed - FeedbackSource->MinimumImpactSpeed, 1.0f);
	const float Strength = FMath::Clamp(
		(ImpactSpeed - FeedbackSource->MinimumImpactSpeed) / Denominator, 0.0f, 1.0f);
	FeedbackSource->OnDiceImpact.Broadcast(Strength, Hit.ImpactPoint);

	const double CurrentTime = FeedbackSource->GetWorld()->GetTimeSeconds();
	if (InOutLastFeedbackTime >= 0.0
		&& CurrentTime - InOutLastFeedbackTime < FMath::Max(FeedbackSource->ImpactFeedbackCooldown, 0.0f))
	{
		return false;
	}
	InOutLastFeedbackTime = CurrentTime;

	if (IsValid(FeedbackSource->BoardImpactSound))
	{
		const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
		const float VolumeMultiplier = FeedbackSource->BoardImpactSoundVolume
			* FMath::Lerp(0.35f, 1.0f, SafeStrength);
		const float MinPitch = FMath::Min(
			FeedbackSource->BoardImpactSoundPitchMin, FeedbackSource->BoardImpactSoundPitchMax);
		const float MaxPitch = FMath::Max(
			FeedbackSource->BoardImpactSoundPitchMin, FeedbackSource->BoardImpactSoundPitchMax);
		UGameplayStatics::PlaySoundAtLocation(
			FeedbackSource,
			FeedbackSource->BoardImpactSound,
			Hit.ImpactPoint,
			VolumeMultiplier,
			FMath::FRandRange(MinPitch, MaxPitch),
			0.0f,
			FeedbackSource->BoardImpactSoundAttenuation);
	}

	if (FeedbackSource->bEnableBoardImpactCameraShake && FeedbackSource->BoardImpactCameraShakeClass)
	{
		if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(FeedbackSource, 0))
		{
			const float ShakeScale = FeedbackSource->BoardImpactCameraShakeScale
				* FMath::Lerp(0.25f, 1.0f, Strength);
			CameraManager->StartCameraShake(FeedbackSource->BoardImpactCameraShakeClass, ShakeScale);
		}
	}
	return true;
}
