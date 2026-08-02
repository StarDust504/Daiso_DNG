// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DataStructs.generated.h"

USTRUCT(BlueprintType)
struct FComboData : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(editAnywhere, BlueprintReadWrite)
	int32 ComboScore;
};
