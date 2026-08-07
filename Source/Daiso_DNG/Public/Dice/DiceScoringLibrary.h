// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Dice/DiceScoringTypes.h"
#include "DiceScoringLibrary.generated.h"

class UDataTable;

/** Blueprint-библиотека чистых функций подсчёта очков за один бросок. */
UCLASS()
class DAISO_DNG_API UDiceScoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Находит самое выгодное разбиение на непересекающиеся комбинации из ScoringRules.
	 * DiceValues должен содержать ровно шесть значений в диапазоне от 1 до 6.
	 */
	UFUNCTION(BlueprintPure, Category="Dice|Scoring", meta=(DisplayName="Calculate Dice Roll Score"))
	static FDiceRollScoreResult CalculateDiceRollScore(
		const TArray<int32>& DiceValues,
		UDataTable* ScoringRules);

	/** Рассчитывает выбранное игроком подмножество, содержащее от одного до шести кубиков. */
	UFUNCTION(BlueprintPure, Category="Dice|Scoring", meta=(DisplayName="Calculate Selected Dice Score"))
	static FDiceRollScoreResult CalculateSelectedDiceScore(
		const TArray<int32>& SelectedDiceValues,
		UDataTable* ScoringRules);
};
