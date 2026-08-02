// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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
	int32 GetCurrentScore(FName Combo);
	
private:
	
	TArray<int32> TempScore;
};
