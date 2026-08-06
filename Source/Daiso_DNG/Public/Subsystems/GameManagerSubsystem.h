// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Daiso_DNG/Public/Dice/CPP_Dice.h"
#include "GameManagerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class DAISO_DNG_API UGameManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Score")
	FName AddComboToTempArray(int32 NumberToAppend);
	
	UFUNCTION(BlueprintCallable, Category = "Score")
	FName RemoveComboFromTempArray(int32 NumberToRemove);
	
	UFUNCTION(BlueprintCallable, Category = "Score")
	int32 GetCurrentScore(FName Combo);
	
	UFUNCTION(BlueprintCallable, Category = "Score")
	void RegisterDice(ACPP_Dice* DiceToRegister);
	
	UFUNCTION(BlueprintCallable, Category = "Score")
	void UnregisterDice(ACPP_Dice* DiceToUnregister);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Score")
	FORCEINLINE bool CheckIsDiceRegistered(ACPP_Dice* DiceToCheck) { return RegisteredDice.Contains(DiceToCheck); };
	
	
	
	UFUNCTION(BlueprintCallable, Category = "Data")
	FORCEINLINE void RegisterScoreDataTable(UDataTable* ScoreDT) { ScoreDataTable = ScoreDT; };
private:
	TArray<int32> TempScore;
	TArray<ACPP_Dice*> RegisteredDice;
	
	UDataTable* ScoreDataTable = nullptr;
};
