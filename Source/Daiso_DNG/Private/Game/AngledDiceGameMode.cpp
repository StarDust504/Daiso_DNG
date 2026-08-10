// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/AngledDiceGameMode.h"

#include "Dice/CPP_Dice.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UI/AngledDiceHUD.h"

/** Собирает минимальный runtime-контур без изменения GM_Game основной сцены. */
AAngledDiceGameMode::AAngledDiceGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = AAngledDiceHUD::StaticClass();
}

/** Подавляет только стартовый Blueprint-бросок в этом GameMode, не меняя BP_Dice основной игры. */
void AAngledDiceGameMode::StartPlay()
{
	TArray<TObjectPtr<ACPP_Dice>> SceneDice;
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		ACPP_Dice* Dice = *It;
		if (IsValid(Dice))
		{
			Dice->SetCanRollDice(false);
			Dice->SetIsActive(false);
			SceneDice.Add(Dice);
		}
	}

	// Super::StartPlay запускает BeginPlay всех акторов мира; в этот момент RollDice отклоняется самим BP_Dice.
	Super::StartPlay();

	for (ACPP_Dice* Dice : SceneDice)
	{
		if (IsValid(Dice))
		{
			Dice->SetCanRollDice(true);
			Dice->SetIsActive(false);
		}
	}
}
