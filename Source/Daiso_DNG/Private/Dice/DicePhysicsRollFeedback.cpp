// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DicePhysicsRollComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

// Воспроизводит пространственный звук и перезапускает лёгкую тряску камеры для локального игрока.
void UDicePhysicsRollComponent::PlayBoardImpactFeedback(const float Strength, const FVector& ImpactLocation)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (LastImpactFeedbackTime >= 0.0
		&& CurrentTime - LastImpactFeedbackTime < FMath::Max(ImpactFeedbackCooldown, 0.0f))
	{
		return;
	}
	LastImpactFeedbackTime = CurrentTime;

	const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	if (IsValid(BoardImpactSound))
	{
		const float VolumeMultiplier = BoardImpactSoundVolume * FMath::Lerp(0.35f, 1.0f, SafeStrength);
		const float MinPitch = FMath::Min(BoardImpactSoundPitchMin, BoardImpactSoundPitchMax);
		const float MaxPitch = FMath::Max(BoardImpactSoundPitchMin, BoardImpactSoundPitchMax);
		UGameplayStatics::PlaySoundAtLocation(
			this,
			BoardImpactSound,
			ImpactLocation,
			VolumeMultiplier,
			FMath::FRandRange(MinPitch, MaxPitch),
			0.0f,
			BoardImpactSoundAttenuation);
	}

	if (bEnableBoardImpactCameraShake && BoardImpactCameraShakeClass)
	{
		if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const float ShakeScale = BoardImpactCameraShakeScale * FMath::Lerp(0.25f, 1.0f, SafeStrength);
			CameraManager->StartCameraShake(BoardImpactCameraShakeClass, ShakeScale);
		}
	}
}
