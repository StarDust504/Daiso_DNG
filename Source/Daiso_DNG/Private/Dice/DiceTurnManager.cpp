// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DiceTurnManager.h"

#include "Dice/DiceRollScoreCollector.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

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
