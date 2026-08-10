// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dice/DiceScoringTypes.h"
#include "GameFramework/Actor.h"
#include "DiceTurnManager.generated.h"

class ADiceRollScoreCollector;

/** The side currently allowed to act in the dice game. */
UENUM(BlueprintType)
enum class EDiceTurnOwner : uint8
{
	Player UMETA(DisplayName="Player"),
	AI UMETA(DisplayName="AI")
};

/** Why the active turn changed. Useful for UI, audio, and AI Blueprint logic. */
UENUM(BlueprintType)
enum class EDiceTurnChangeReason : uint8
{
	GameStarted UMETA(DisplayName="Game Started"),
	PlayerEndedTurn UMETA(DisplayName="Player Ended Turn"),
	PlayerBust UMETA(DisplayName="Player Bust"),
	AIEndedTurn UMETA(DisplayName="AI Ended Turn"),
	Manual UMETA(DisplayName="Manual")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDiceTurnChangedSignature, EDiceTurnOwner, PreviousTurn, EDiceTurnOwner, NewTurn,
	EDiceTurnChangeReason, Reason);

/**
 * Owns the player/AI turn state. Place one in the level, choose InitialTurn in
 * its Details panel, and have AI Blueprint logic call EndAITurn when finished.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API ADiceTurnManager : public AActor
{
	GENERATED_BODY()

public:
	ADiceTurnManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Select who takes the very first turn in the placed actor's Details panel. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Dice|Turns")
	EDiceTurnOwner InitialTurn = EDiceTurnOwner::Player;

	/** Optional explicit collector. Leave unset to use the first collector in the level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Dice|Turns|Setup")
	TObjectPtr<ADiceRollScoreCollector> ScoreCollector;

	/** When enabled, finds the first DiceRollScoreCollector if ScoreCollector was not assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Turns|Setup")
	bool bAutoFindScoreCollector = true;

	/** Read-only current state; use this to gate player controls or update turn UI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Turns")
	EDiceTurnOwner ActiveTurn = EDiceTurnOwner::Player;

	/** Fires whenever ownership changes, including the initial turn selected at game start. */
	UPROPERTY(BlueprintAssignable, Category="Dice|Turns|Events")
	FDiceTurnChangedSignature OnTurnChanged;

	/** Ends the player turn. This is also bound to the Q key while the player is active. */
	UFUNCTION(BlueprintCallable, Category="Dice|Turns")
	bool EndPlayerTurn();

	/** Call this from AI Blueprint logic once the AI has completed its turn. */
	UFUNCTION(BlueprintCallable, Category="Dice|Turns")
	bool EndAITurn();

	/** Explicitly selects the active side; intended for setup/restarts and debugging. */
	UFUNCTION(BlueprintCallable, Category="Dice|Turns")
	void SetActiveTurn(EDiceTurnOwner NewTurn, EDiceTurnChangeReason Reason = EDiceTurnChangeReason::Manual);

	/** True only while player input and player dice actions should be accepted. */
	UFUNCTION(BlueprintPure, Category="Dice|Turns")
	bool IsPlayerTurn() const { return ActiveTurn == EDiceTurnOwner::Player; }

	/** True while AI decision/roll logic should be running. */
	UFUNCTION(BlueprintPure, Category="Dice|Turns")
	bool IsAITurn() const { return ActiveTurn == EDiceTurnOwner::AI; }
	
	UFUNCTION(BlueprintCallable, Category="Dice|Turns")
	void SwitchDiceOnTurnSwitch();

private:
	/** Receives the completed six-die score from the collector to detect a player bust. */
	UFUNCTION()
	void HandleRollScored(FDiceRollScoreResult ScoreResult);

	/** Raw Q-key callback; delegates validation to EndPlayerTurn. */
	void HandleEndTurnKey();
	/** Uses the explicitly assigned collector or, when enabled, locates one in the level. */
	void ResolveScoreCollector();
};
