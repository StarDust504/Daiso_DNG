// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LevelProgressTypes.generated.h"

/** Одна строка таблицы целей: номер уровня и необходимый результат раунда. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FLevelGoalRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level Progress", meta=(ClampMin="1"))
	int32 LevelNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level Progress", meta=(ClampMin="1"))
	int32 TargetScore = 1500;
};

/** Снимок состояния уровня, который используется UI и Blueprint-логикой. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FLevelProgressState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level Progress")
	int32 LevelNumber = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level Progress")
	int32 TargetScore = 1500;

	/** Динамический результат выбранных и сохранённых в текущем раунде костей. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level Progress")
	int32 CurrentScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level Progress")
	bool bCanFinishRound = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level Progress")
	bool bLevelWon = false;
};
