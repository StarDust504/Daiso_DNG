// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/AngledDiceGameMode.h"
#include "TrajectoryDiceGameMode.generated.h"

class AMouseGatherDiceManager;
class ASpectacleDiceRollManager;

/** Владелец только двух менеджеров, используемых новой полноценной игровой сценой. */
UCLASS()
class DAISO_DNG_API ATrajectoryDiceGameMode : public AAngledDiceGameMode
{
	GENERATED_BODY()

public:
	ATrajectoryDiceGameMode();
	virtual void StartPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMouseGatherDiceManager> MouseGatherManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ASpectacleDiceRollManager> SpectacleRollManager = nullptr;
};
