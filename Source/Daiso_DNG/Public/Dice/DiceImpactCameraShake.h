// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "DiceImpactCameraShake.generated.h"

/** Короткая ненавязчивая тряска камеры для удара кубика о доску. */
UCLASS()
class DAISO_DNG_API UDiceImpactCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	/** Создаёт короткий Perlin-шаблон с малой амплитудой перемещения и поворота. */
	UDiceImpactCameraShake(const FObjectInitializer& ObjectInitializer);
};
