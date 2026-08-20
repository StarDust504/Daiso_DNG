// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DiceTurnManager.h"

#include "Dice/DiceRollScoreCollector.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

#include "Daiso_DNG/Private/FunctionLibraries/CPP_CommonFunctionLibrary.cpp"
#include "Dice/CPP_Dice.h"
#include "Dice/Setup/CPP_DiceSpawner.h"
#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
#include "Player/CPP_Player.h"

ADiceTurnManager::ADiceTurnManager()
{
	// The manager is event-driven: it reacts to input and completed rolls, so it does not need Tick.
	PrimaryActorTick.bCanEverTick = false;
}

void ADiceTurnManager::BeginPlay()
{
	Super::BeginPlay();

	// Subscribe before play begins so the first completed roll can immediately trigger a bust handoff.
	// A manually assigned collector takes priority over the optional level search.
	ResolveScoreCollector();
	if (IsValid(ScoreCollector))
	{
		ScoreCollector->OnRollScored.AddUniqueDynamic(this, &ADiceTurnManager::HandleRollScored);
	}

	// The key is bound directly on this level actor instead of requiring an input mapping asset.
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		EnableInput(PlayerController);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADiceTurnManager::HandleEndTurnKey);
		}
	}

	// Publish the selected starting side so UI and AI listeners can initialize themselves.
	SetActiveTurn(InitialTurn, EDiceTurnChangeReason::GameStarted);
	
	InitialDiceSpawn();
}

void ADiceTurnManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Prevent the collector retaining a callback to this actor during level teardown.
	if (IsValid(ScoreCollector))
	{
		ScoreCollector->OnRollScored.RemoveDynamic(this, &ADiceTurnManager::HandleRollScored);
	}

	// Remove the Q binding created in BeginPlay.
	DisableInput(nullptr);
	Super::EndPlay(EndPlayReason);
}

bool ADiceTurnManager::EndPlayerTurn()
{
	// Q is ignored while AI logic owns the turn.
	if (!IsPlayerTurn())
	{
		return false;
	}

	SetActiveTurn(EDiceTurnOwner::AI, EDiceTurnChangeReason::PlayerEndedTurn);
	return true;
}

bool ADiceTurnManager::EndAITurn()
{
	// Prevent an old/delayed AI callback from ending a newer player turn.
	if (!IsAITurn())
	{
		return false;
	}

	SetActiveTurn(EDiceTurnOwner::Player, EDiceTurnChangeReason::AIEndedTurn);
	return true;
}

void ADiceTurnManager::SetActiveTurn(const EDiceTurnOwner NewTurn, const EDiceTurnChangeReason Reason)
{
	// Keep the previous owner for listeners that need to animate or clean up the outgoing turn.
	const EDiceTurnOwner PreviousTurn = ActiveTurn;
	ActiveTurn = NewTurn;
	OnTurnChanged.Broadcast(PreviousTurn, ActiveTurn, Reason);
}

void ADiceTurnManager::InitialDiceSpawn()
{
	ACPP_Player* PlayerPawn = Cast<ACPP_Player>(UGameplayStatics::GetPlayerPawn(this, 0));
	
	if (!PlayerPawn && !Enemy)
		return;
	
	
	
	PlayerDiceArray.Empty();
	EnemyDiceArray.Empty();
	
	for (ACPP_DiceSpawner* Spawner : UCPP_CommonFunctionLibrary::GetAllActorsOfClass<ACPP_DiceSpawner>(this))
	{
		if (Spawner->SpawnerType == ESpawnerType::PLAYER)
			PlayerDiceSpawnerArray.Add(Spawner);
		else if (Spawner->SpawnerType == ESpawnerType::AI)
			EnemyDiceSpawnerArray.Add(Spawner);
		else
			BoardDiceSpawnerArray.Add(Spawner);
	}
	
	
	for (int32 i = 0; i < PlayerPawn->DicePreset.CurrentDiceCount; i++)
	{
		ACPP_Dice* TempDice = GetWorld()->SpawnActor<ACPP_Dice>(PlayerPawn->DicePreset.DicePreset, PlayerDiceSpawnerArray[i]->GetActorLocation(), PlayerDiceSpawnerArray[i]->GetActorRotation());
		PlayerDiceArray.Add(TempDice);
	}
	
	for (int32 i = 0; i < Enemy->DicePreset.CurrentDiceCount; i++)
	{
		ACPP_Dice* TempDice = GetWorld()->SpawnActor<ACPP_Dice>(Enemy->DicePreset.DicePreset, EnemyDiceSpawnerArray[i]->GetActorLocation(), EnemyDiceSpawnerArray[i]->GetActorRotation());
		EnemyDiceArray.Add(TempDice);
	}
	
	switch (InitialTurn)
	{
		case EDiceTurnOwner::AI:
		SetDicePosition(InitialTurn, ESpawnerType::BOARD);
		SetDicePosition(EDiceTurnOwner::Player, ESpawnerType::PLAYER);
		break;
		
		case EDiceTurnOwner::Player:
		SetDicePosition(InitialTurn, ESpawnerType::BOARD);
		SetDicePosition(EDiceTurnOwner::AI, ESpawnerType::PLAYER);
		break;
	}
}

void ADiceTurnManager::SetDicePosition(EDiceTurnOwner TurnOwner, ESpawnerType SpawnerType)
{
	switch (TurnOwner)
	{
	case EDiceTurnOwner::Player:
			for (int32 i = 0; i < PlayerDiceArray.Num(); i++)
			{
				switch (SpawnerType)
				{
					case ESpawnerType::PLAYER:
						PlayerDiceArray[i]->SetActorLocation(PlayerDiceSpawnerArray[i]->GetActorLocation());
						break;
					
					case ESpawnerType::BOARD:
						PlayerDiceArray[i]->SetActorLocation(BoardDiceSpawnerArray[i]->GetActorLocation());
						break;
				}
			}
			break;
		
		case EDiceTurnOwner::AI:
			for (int32 i = 0; i < EnemyDiceArray.Num(); i++)
			{
				switch (SpawnerType)
				{
				case ESpawnerType::PLAYER:
					EnemyDiceArray[i]->SetActorLocation(EnemyDiceSpawnerArray[i]->GetActorLocation());
					break;
					
				case ESpawnerType::BOARD:
					EnemyDiceArray[i]->SetActorLocation(BoardDiceSpawnerArray[i]->GetActorLocation());
					break;
				}
			}
			break;
	}
}

void ADiceTurnManager::SwitchDiceOnTurnSwitch(EDiceTurnOwner TurnOwner)
{

}

void ADiceTurnManager::ClearAllDice()
{
	
	/*Destroying currently present*/
	TArray<ACPP_Dice*> DiceArray = UCPP_CommonFunctionLibrary::GetAllActorsOfClass<ACPP_Dice>(this);
	
	for (ACPP_Dice* Dice : DiceArray)
	{
		Dice->Destroy();
	}
	
	DiceArray.Empty();
	
	
}

void ADiceTurnManager::HandleRollScored(const FDiceRollScoreResult ScoreResult)
{
	// Only a completed, valid player roll can force this handoff. AI bust handling belongs
	// to the AI behavior, which should call EndAITurn after resolving its own result.
	if (IsPlayerTurn() && ScoreResult.bIsValid && ScoreResult.bIsBust)
	{
		SetActiveTurn(EDiceTurnOwner::AI, EDiceTurnChangeReason::PlayerBust);
	}
}

void ADiceTurnManager::HandleEndTurnKey()
{
	// EndPlayerTurn contains the player-turn guard, so Q is harmless during the AI turn.
	EndPlayerTurn();
}

void ADiceTurnManager::ResolveScoreCollector()
{
	// An assigned collector is always used. Do not search if auto-discovery is disabled
	// or if this function is called without an active world.
	if (IsValid(ScoreCollector) || !bAutoFindScoreCollector || !IsValid(GetWorld()))
	{
		return;
	}

	// This project normally has one collector. If several exist, assign ScoreCollector
	// explicitly to avoid depending on level actor iteration order.
	for (TActorIterator<ADiceRollScoreCollector> It(GetWorld()); It; ++It)
	{
		ScoreCollector = *It;
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s could not find a DiceRollScoreCollector; busts will not end the player turn."), *GetName());
}
