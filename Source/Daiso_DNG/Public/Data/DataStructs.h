// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DataStructs.generated.h"

class ACPP_Dice;

USTRUCT(BlueprintType)
struct FComboData : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(editAnywhere, BlueprintReadWrite)
	int32 ComboScore = 0;
};

USTRUCT(BlueprintType)
struct FDiceSetup
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice")
	TSubclassOf<ACPP_Dice> DicePreset;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dice")
	int32 CurrentDiceCount = 6;
	
	
};