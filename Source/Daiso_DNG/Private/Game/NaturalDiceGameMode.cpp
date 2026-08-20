// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/NaturalDiceGameMode.h"

#include "Dice/MouseGatherDiceManager.h"
#include "Dice/NaturalDiceRollManager.h"
#include "Dice/SpectacleDiceRollManager.h"
#include "Player/DiceRollPlayerController.h"
#include "UI/SpectacleDiceHUD.h"

ANaturalDiceGameMode::ANaturalDiceGameMode()
{
	PlayerControllerClass = ADiceRollPlayerController::StaticClass();
	HUDClass = ASpectacleDiceHUD::StaticClass();
}

void ANaturalDiceGameMode::StartPlay()
{
	// Spawn before Super::StartPlay so the HUD can bind to the manager during its BeginPlay.
	if (GetWorld())
	{
		NaturalRollManager = GetWorld()->SpawnActor<ANaturalDiceRollManager>();
		MouseGatherManager = GetWorld()->SpawnActor<AMouseGatherDiceManager>();
		SpectacleRollManager = GetWorld()->SpawnActor<ASpectacleDiceRollManager>();
	}
	Super::StartPlay();
}
