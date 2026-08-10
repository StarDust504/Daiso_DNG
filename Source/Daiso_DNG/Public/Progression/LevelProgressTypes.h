// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Progression/BoostTypes.h"
#include "LevelProgressTypes.generated.h"

/** Одна строка DT_RunStages, перенесённая из одноимённого листа Balance_dice.xlsx. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FRunStageRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="1"))
	int32 Round = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="1"))
	int32 RequiredInvoice = 1500;

	/** Текстовый коэффициент роста сохранён для дизайнерской диагностики и не участвует в расчётах. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress")
	FString Growth = TEXT("-");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress")
	int32 Win = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress")
	int32 Lose = -4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="0"))
	int32 Store = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="0.0"))
	float Common = 0.94f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="0.0"))
	float Rare = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="0.0"))
	float Epic = 0.008f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run Progress", meta=(ClampMin="0.0"))
	float Legendary = 0.002f;
};

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	int32 Money = 25;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	bool bInStore = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	bool bGameOver = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	bool bLastRoundWon = false;
};

/** Полный Blueprint-доступный снимок состояния одного забега. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FRunProgressState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	int32 RoundNumber = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	int32 RequiredInvoice = 1500;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	int32 Money = 25;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	bool bStoreOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	bool bGameOver = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	bool bLastRoundWon = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	TArray<FOwnedBoostStack> OwnedBoosts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run Progress")
	TArray<FBoostStoreOffer> StoreOffers;
};
