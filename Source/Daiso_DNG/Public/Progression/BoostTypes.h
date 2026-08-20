// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dice/DiceScoringTypes.h"
#include "Engine/DataTable.h"
#include "BoostTypes.generated.h"

/** Редкость буста; значения напрямую соответствуют колонке Rarity в DT_Boosts. */
UENUM(BlueprintType)
enum class EBoostRarity : uint8
{
	Common UMETA(DisplayName="Common"),
	Rare UMETA(DisplayName="Rare"),
	Epic UMETA(DisplayName="Epic"),
	Legendary UMETA(DisplayName="Legendary")
};

/** Игровой момент, в который строка DT_Boosts имеет право применить свой эффект. */
UENUM(BlueprintType)
enum class EBoostEffectTrigger : uint8
{
	None UMETA(DisplayName="None"),
	ScoreCalculated UMETA(DisplayName="Score Calculated"),
	Farkle UMETA(DisplayName="Farkle"),
	HotDice UMETA(DisplayName="Hot Dice"),
	RollResolved UMETA(DisplayName="Roll Resolved"),
	RoundFinished UMETA(DisplayName="Round Finished"),
	BoostPurchased UMETA(DisplayName="Boost Purchased"),
	RunWon UMETA(DisplayName="Run Won")
};

/**
 * Универсальная операция эффекта. Новые бусты добавляются данными, пока их поведение
 * укладывается в одну из операций; новые классы операций расширяют один исполнитель,
 * не затрагивая магазин, экономику или хранение стаков.
 */
UENUM(BlueprintType)
enum class EBoostEffectOperation : uint8
{
	None UMETA(DisplayName="None"),
	AddBasePerMatchingDie UMETA(DisplayName="Add Base Per Matching Die"),
	AddBasePerCombination UMETA(DisplayName="Add Base Per Combination"),
	AddBasePerSet UMETA(DisplayName="Add Base Per Set"),
	AddBasePerStraight UMETA(DisplayName="Add Base Per Straight"),
	AddBaseIfScoreAtLeast UMETA(DisplayName="Add Base If Score At Least"),
	AddMultiplier UMETA(DisplayName="Add Multiplier"),
	AddMultiplierPerCombination UMETA(DisplayName="Add Multiplier Per Combination"),
	AddMultiplierPerFaceMilestone UMETA(DisplayName="Add Multiplier Per Face Milestone"),
	AddMultiplierPerPreviousSet UMETA(DisplayName="Add Multiplier Per Previous Set"),
	AddMultiplierPerHotDice UMETA(DisplayName="Add Multiplier Per Hot Dice"),
	AddMultiplierPerStraight UMETA(DisplayName="Add Multiplier Per Straight"),
	AddMultiplierPerMoneyBlock UMETA(DisplayName="Add Multiplier Per Money Block"),
	AddNextRoundPurchaseMultiplier UMETA(DisplayName="Add Next Round Purchase Multiplier"),
	AddMultiplierPerUniqueCombination UMETA(DisplayName="Add Multiplier Per Unique Combination"),
	AddMultiplierPerSuccessfulReroll UMETA(DisplayName="Add Multiplier Per Successful Reroll"),
	GrantMoneyPerScoreBlock UMETA(DisplayName="Grant Money Per Score Block"),
	MultiplyScore UMETA(DisplayName="Multiply Score"),
	RetriggerEveryNthSet UMETA(DisplayName="Retrigger Every Nth Set"),
	MultiplyPerOwnedBoost UMETA(DisplayName="Multiply Per Owned Boost"),
	MultiplyPerSuccessAfterThreshold UMETA(DisplayName="Multiply Per Success After Threshold"),
	MultiplyNextFullCycleAfterHotDice UMETA(DisplayName="Multiply Next Full Cycle After Hot Dice"),
	MultiplyPerMoneyBlock UMETA(DisplayName="Multiply Per Money Block"),
	MultiplyFirstCombinationAfterPurchase UMETA(DisplayName="Multiply First Combination After Purchase"),
	MultiplyPerSuccessfulRoll UMETA(DisplayName="Multiply Per Successful Roll"),
	RetriggerLastCombinationOnHotDice UMETA(DisplayName="Retrigger Last Combination On Hot Dice"),
	RecursiveFirstRetrigger UMETA(DisplayName="Recursive First Retrigger"),
	MultiplyPerWin UMETA(DisplayName="Multiply Per Win"),
	MultiplyAfterUniqueCombinationTypes UMETA(DisplayName="Multiply After Unique Combination Types"),
	EnhanceNextXMultiplier UMETA(DisplayName="Enhance Next X Multiplier"),
	MultiplyPerBoostMoneyTrigger UMETA(DisplayName="Multiply Per Boost Money Trigger"),
	PreserveTurnScoreFraction UMETA(DisplayName="Preserve Turn Score Fraction")
};

/** Одна строка DT_Boosts, содержащая витринные данные и машиночитаемое описание эффекта. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FBoostRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost")
	EBoostRarity Rarity = EBoostRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost", meta=(ClampMin="0"))
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost")
	FText EffectDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost", meta=(ClampMin="1"))
	int32 MaxStacks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost")
	FString Tags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	EBoostEffectTrigger EffectTrigger = EBoostEffectTrigger::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	EBoostEffectOperation EffectOperation = EBoostEffectOperation::None;

	/** Основное числовое значение операции: Base, +Mult, XMult либо доля 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	float EffectMagnitude = 0.0f;

	/** Необязательный порог срабатывания, например минимальный текущий Mult. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	float EffectThreshold = 0.0f;

	/** Необязательная грань 1..6 для операций, считающих подходящие кости. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect", meta=(ClampMin="0", ClampMax="6"))
	int32 EffectFaceValue = 0;

	/** Второе значение для эффектов с усиленным вариантом, например полного стрита. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	float EffectSecondaryMagnitude = 0.0f;

	/** Необязательный общий предел накопления эффекта; ноль означает отсутствие предела. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boost|Effect", meta=(ClampMin="0"))
	int32 EffectLimit = 0;
};

/** Купленный буст и число его стаков в текущем забеге. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FOwnedBoostStack
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	FName BoostId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	int32 StackCount = 0;
};

/** Полностью подготовленная для UI карточка одного предложения магазина. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FBoostStoreOffer
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	FName BoostId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	EBoostRarity Rarity = EBoostRarity::Common;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	int32 Cost = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	FText EffectDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	int32 CurrentStacks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	int32 MaxStacks = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	bool bCanPurchase = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost")
	bool bPurchased = false;
};

/** Входные данные универсального исполнителя бустов для одного результата scoring. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FBoostEffectContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	int32 BaseScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect", meta=(ClampMin="0.0"))
	float BaseMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	TArray<int32> ScoredDiceValues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect", meta=(ClampMin="0"))
	int32 CombinationCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	TArray<EDiceScoringCombinationType> CombinationTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	TArray<FName> CombinationRuleNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	TArray<int32> CombinationScores;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	TArray<int32> CombinationDiceCounts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect", meta=(ClampMin="0"))
	int32 CurrentTurnScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	bool bIsFarkle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boost|Effect")
	bool bIsHotDice = false;
};

/** Результат последовательного применения Base, +Mult, XMult и специальных эффектов. */
USTRUCT(BlueprintType)
struct DAISO_DNG_API FBoostEffectResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	int32 ModifiedBaseScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	float AdditiveMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	float XMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	int32 FinalScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	int32 PreservedTurnScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	int32 RetriggeredBaseScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	int32 RetriggerCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boost|Effect")
	int32 ActivatedXMultiplierCount = 0;
};
