// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dice/DiceScoringTypes.h"
#include "GameFramework/Actor.h"
#include "DiceRollScoreCollector.generated.h"

class UDataTable;
class UDicePhysicsRollComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDiceRollScoredSignature, FDiceRollScoreResult, ScoreResult);

/**
 * Собирает результаты OnDiceRollFinished от шести кубиков и рассчитывает результат броска.
 * При пустом DiceActors автоматически подключается ко всем компонентам DicePhysicsRoll на уровне.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API ADiceRollScoreCollector : public AActor
{
	GENERATED_BODY()

public:
	/** Создаёт коллектор и назначает стандартную таблицу правил подсчёта. */
	ADiceRollScoreCollector();

	/** Очищает промежуточные значения и подключается к кубикам при запуске игры. */
	virtual void BeginPlay() override;

	/** Снимает подписки с кубиков перед уничтожением актора. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Optional explicit dice. Leave empty to find the six dice in the current level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Dice|Scoring|Setup")
	TArray<TObjectPtr<AActor>> DiceActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Scoring|Setup")
	bool bAutoFindDiceActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Scoring|Setup")
	TObjectPtr<UDataTable> ScoringRules;

	/** Prints the calculated score after every completed six-dice roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Scoring|Debug")
	bool bPrintScoreToScreen = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring|State")
	TArray<int32> CurrentRollResults;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dice|Scoring|State")
	FDiceRollScoreResult LastScoreResult;

	UPROPERTY(BlueprintAssignable, Category="Dice|Scoring|Events")
	FDiceRollScoredSignature OnRollScored;

	/** Повторно находит и подключает кубики; возвращает true только при подключении ровно шести. */
	UFUNCTION(BlueprintCallable, Category="Dice|Scoring")
	bool ConnectToDice();

	/** Очищает незавершённый бросок, не удаляя последний рассчитанный результат. */
	UFUNCTION(BlueprintCallable, Category="Dice|Scoring")
	void ResetCollectedRoll();

	/** Принимает одно значение вручную; физический бросок вызывает этот путь через OnDiceRollFinished. */
	UFUNCTION(BlueprintCallable, Category="Dice|Scoring")
	bool SubmitDieResult(int32 Result);

	/** Возвращает количество компонентов кубиков, от которых сейчас принимаются результаты. */
	UFUNCTION(BlueprintPure, Category="Dice|Scoring")
	int32 GetConnectedDiceCount() const;

private:
	static constexpr int32 DicePerRoll = 6;

	/** Передаёт завершившееся значение кубика в общий накопитель броска. */
	UFUNCTION()
	void HandleDiceRollFinished(int32 Result);

	/** Снимает все динамические подписки и очищает список подключённых компонентов. */
	void UnbindDice();

	/** Рассчитывает собранные шесть значений, публикует событие и выводит диагностику. */
	void PublishScore();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDicePhysicsRollComponent>> BoundDiceComponents;
};
