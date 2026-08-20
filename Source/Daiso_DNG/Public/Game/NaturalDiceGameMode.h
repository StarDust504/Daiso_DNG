// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/AngledDiceGameMode.h"
#include "NaturalDiceGameMode.generated.h"

class ANaturalDiceRollManager;
class AMouseGatherDiceManager;
class ASpectacleDiceRollManager;

/** Keeps the isolated roll scene intact and adds the managers and HUD used only for dice-roll experiments. */
UCLASS()
class DAISO_DNG_API ANaturalDiceGameMode : public AAngledDiceGameMode
{
	GENERATED_BODY()

public:
	ANaturalDiceGameMode();
	virtual void StartPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ANaturalDiceRollManager> NaturalRollManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AMouseGatherDiceManager> MouseGatherManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ASpectacleDiceRollManager> SpectacleRollManager = nullptr;
};
