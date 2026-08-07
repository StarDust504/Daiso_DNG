// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Dice/DiceScoringTypes.h"
#include "GameManagerSubsystem.generated.h"

class ACPP_Dice;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDiceSelectionChangedSignature, FDiceRollScoreResult, ScoreResult);

/** Управляет выбранными игроком кубиками и публикует текущий результат их комбинации. */
UCLASS()
class DAISO_DNG_API UGameManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	/** Загружает таблицу правил подсчёта при создании подсистемы мира. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Добавляет значение выбранного кубика, обновляет счёт и возвращает ключ выбора. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	FName AddComboToTempArray(int32 NumberToAppend);
	
	/** Удаляет одно выбранное значение, обновляет счёт и возвращает ключ выбора. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	FName RemoveComboFromTempArray(int32 NumberToRemove);
	
	/** Возвращает текущий рассчитанный счёт; параметр Combo сохранён для совместимости Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	int32 GetCurrentScore(FName Combo);

	/** Возвращает отсортированные значения выбранных кубиков. */
	UFUNCTION(BlueprintPure, Category = "Score")
	TArray<int32> GetSelectedDiceValues() const;

	/** Возвращает полный результат подсчёта текущего выбора. */
	UFUNCTION(BlueprintPure, Category = "Score")
	FDiceRollScoreResult GetSelectedDiceScore() const;

	/** Возвращает true, если все выбранные кубики входят в результативные комбинации. */
	UFUNCTION(BlueprintPure, Category = "Score")
	bool IsCurrentDiceSelectionValid() const;

	/** Полностью очищает выбор кубиков и публикует обновлённый результат. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void ClearDiceSelection();

	UPROPERTY(BlueprintAssignable, Category = "Score")
	FDiceSelectionChangedSignature OnDiceSelectionChanged;
	
	/** Регистрирует кубик в подсистеме, если он ещё не зарегистрирован. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void RegisterDice(ACPP_Dice* DiceToRegister);
	
	/** Удаляет кубик из списка зарегистрированных. */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void UnregisterDice(ACPP_Dice* DiceToUnregister);
	
	/** Проверяет, содержится ли кубик в списке зарегистрированных. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Score")
	bool CheckIsDiceRegistered(ACPP_Dice* DiceToCheck) const;

	/** Сохраняет старую таблицу очков, используемую существующими Blueprint-графами. */
	UFUNCTION(BlueprintCallable, Category = "Data")
	void RegisterScoreDataTable(UDataTable* ScoreDT);
private:
	/** Строит совместимый ключ из отсортированных значений выбранных кубиков. */
	FName BuildSelectedDiceKey() const;

	/** Пересчитывает текущий выбор, обновляет валидность и рассылает событие Blueprint. */
	void RefreshSelectionScore();

	UPROPERTY(Transient)
	TArray<int32> TempScore;

	UPROPERTY(Transient)
	TArray<ACPP_Dice*> RegisteredDice;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> DiceScoringRules = nullptr;

	UPROPERTY(Transient)
	FDiceRollScoreResult LastSelectionScoreResult;

	UPROPERTY(Transient)
	bool bIsCurrentSelectionValid = false;
	
	UPROPERTY(Transient)
	UDataTable* ScoreDataTable = nullptr;
};
