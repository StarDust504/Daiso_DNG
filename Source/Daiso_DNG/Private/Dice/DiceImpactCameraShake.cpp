// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DiceImpactCameraShake.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"

// Настраивает короткий одиночный импульс, который не накапливается от одновременных ударов кубиков.
UDiceImpactCameraShake::UDiceImpactCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	UPerlinNoiseCameraShakePattern* Pattern = ObjectInitializer.CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(
		this, TEXT("DiceImpactPattern"));
	SetRootShakePattern(Pattern);
	Pattern->Duration = 0.14f;
	Pattern->BlendInTime = 0.01f;
	Pattern->BlendOutTime = 0.08f;

	Pattern->LocationAmplitudeMultiplier = 1.0f;
	Pattern->LocationFrequencyMultiplier = 1.0f;
	Pattern->X.Amplitude = 0.18f;
	Pattern->X.Frequency = 24.0f;
	Pattern->Y.Amplitude = 0.18f;
	Pattern->Y.Frequency = 21.0f;
	Pattern->Z.Amplitude = 0.32f;
	Pattern->Z.Frequency = 26.0f;

	Pattern->RotationAmplitudeMultiplier = 1.0f;
	Pattern->RotationFrequencyMultiplier = 1.0f;
	Pattern->Pitch.Amplitude = 0.12f;
	Pattern->Pitch.Frequency = 22.0f;
	Pattern->Yaw.Amplitude = 0.08f;
	Pattern->Yaw.Frequency = 19.0f;
	Pattern->Roll.Amplitude = 0.10f;
	Pattern->Roll.Frequency = 24.0f;
	Pattern->FOV.Amplitude = 0.0f;
}
