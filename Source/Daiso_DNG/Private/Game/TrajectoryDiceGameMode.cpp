// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/TrajectoryDiceGameMode.h"

#include "Dice/MouseGatherDiceManager.h"
#include "Dice/SpectacleDiceRollManager.h"
#include "Player/DiceRollPlayerController.h"
#include "UI/TrajectoryDiceHUD.h"

ATrajectoryDiceGameMode::ATrajectoryDiceGameMode()
{
	PlayerControllerClass = ADiceRollPlayerController::StaticClass();
	HUDClass = ATrajectoryDiceHUD::StaticClass();
}

void ATrajectoryDiceGameMode::StartPlay()
{
	// HUD подписывается на менеджеры во время общего StartPlay, поэтому создаём их заранее.
	if (GetWorld())
	{
		MouseGatherManager = GetWorld()->SpawnActor<AMouseGatherDiceManager>();
		SpectacleRollManager = GetWorld()->SpawnActor<ASpectacleDiceRollManager>();
	}
	Super::StartPlay();
}
