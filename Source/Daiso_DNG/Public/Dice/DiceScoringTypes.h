// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DiceScoringTypes.generated.h"

UENUM(BlueprintType)
enum class EDiceScoringCombinationType : uint8
{
	SingleFace UMETA(DisplayName="Single Face"),
	SameFace UMETA(DisplayName="Same Face"),
	Straight UMETA(DisplayName="Straight")
};

UENUM(BlueprintType)
enum class EDiceScoreScalingRule : uint8
{
	None UMETA(DisplayName="None"),
	DoublePerAdditionalDie UMETA(DisplayName="Double Per Additional Die")
};

/** One configurable scoring rule. Intended to be used as a Data Table row. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FDiceScoringRule : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	EDiceScoringCombinationType CombinationType = EDiceScoringCombinationType::SingleFace;

	/** Used by SingleFace and SameFace; ignored by Straight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring", meta=(ClampMin="1", ClampMax="6"))
	int32 FaceValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring", meta=(ClampMin="1", ClampMax="6"))
	int32 MinDiceCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring", meta=(ClampMin="1", ClampMax="6"))
	int32 MaxDiceCount = 1;

	/** Inclusive bounds used by Straight; ignored by other rule types. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring", meta=(ClampMin="1", ClampMax="6"))
	int32 StraightStart = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring", meta=(ClampMin="1", ClampMax="6"))
	int32 StraightEnd = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring", meta=(ClampMin="0"))
	int32 BaseScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	EDiceScoreScalingRule ScalingRule = EDiceScoreScalingRule::None;

	/** Resolves equal-score partitions only. It never overrides a higher score. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct DAISO_DNG_API FDiceScoringCombination
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	FName RuleRowName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	EDiceScoringCombinationType CombinationType = EDiceScoringCombinationType::SingleFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	TArray<int32> DiceValues;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	int32 Score = 0;
};

USTRUCT(BlueprintType)
struct DAISO_DNG_API FDiceRollScoreResult
{
	GENERATED_BODY()

	/** False means the input or the supplied Data Table was invalid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	bool bIsValid = false;

	/** True only for a valid roll with no scoring combination at all. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	bool bIsBust = false;

	/** True when every supplied die belongs to one of the selected scoring combinations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	bool bAllDiceScored = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	int32 TotalScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	TArray<FDiceScoringCombination> Combinations;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	TArray<int32> UnscoredDiceValues;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring")
	FString ErrorMessage;
};
