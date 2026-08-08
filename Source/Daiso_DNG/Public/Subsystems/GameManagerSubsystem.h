// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Dice/DiceScoringTypes.h"
#include "Progression/LevelProgressTypes.h"
#include "GameManagerSubsystem.generated.h"

class ACPP_Dice;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDiceSelectionChangedSignature, FDiceRollScoreResult, ScoreResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FLevelProgressChangedSignature, FLevelProgressState, ProgressState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FLevelWonSignature, FLevelProgressState, ProgressState);

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

	/** Вызывается при изменении выбранных костей, текущего счёта, цели или состояния победы. */
	UPROPERTY(BlueprintAssignable, Category = "Level Progress")
	FLevelProgressChangedSignature OnLevelProgressChanged;

	/** Вызывается, когда завершённый раунд достигает текущей цели уровня. */
	UPROPERTY(BlueprintAssignable, Category = "Level Progress")
	FLevelWonSignature OnLevelWon;

	/** Возвращает текущий снимок счёта и цели уровня. */
	UFUNCTION(BlueprintPure, Category = "Level Progress")
	FLevelProgressState GetLevelProgress() const;

	/** Завершает раунд, проверяет его результат, сбрасывает счёт и при успехе загружает следующую цель. */
	UFUNCTION(BlueprintCallable, Category = "Level Progress")
	bool FinishRound();

	/** Переключает текущий уровень на строку из DT_LevelGoals и сбрасывает игровой прогресс. */
	UFUNCTION(BlueprintCallable, Category = "Level Progress")
	bool SetCurrentLevelNumber(int32 NewLevelNumber);

	/** Сохраняет старую таблицу очков, используемую существующими Blueprint-графами. */
	UFUNCTION(BlueprintCallable, Category = "Data")
	void RegisterScoreDataTable(UDataTable* ScoreDT);

	/** Регистрирует кубик в подсистеме, если он ещё не зарегистрирован. */
	UFUNCTION(BlueprintCallable, Category = "Player")
	void RegisterDice(ACPP_Dice* DiceToRegister);
	
	/** Удаляет кубик из списка зарегистрированных. */
	UFUNCTION(BlueprintCallable, Category = "Player")
	void UnregisterDice(ACPP_Dice* DiceToUnregister);
	
	/** Проверяет, содержится ли кубик в списке зарегистрированных. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player")
	bool CheckIsDiceRegistered(ACPP_Dice* DiceToCheck) const;
	
	UFUNCTION(BlueprintCallable, Category = "Player")
	void DestroyRegisteredDice();
private:
	/** Строит совместимый ключ из отсортированных значений выбранных кубиков. */
	FName BuildSelectedDiceKey() const;

	/** Пересчитывает текущий выбор, обновляет валидность и рассылает событие Blueprint. */
	void RefreshSelectionScore();

	/** Загружает цель текущего уровня из отдельной Data Table. */
	bool LoadCurrentLevelGoal();

	/** Публикует актуальное состояние уровня всем подписчикам. */
	void PublishLevelProgress();

	/** Снимает выбор и возвращает зарегистрированные кубики в состояние нового раунда. */
	void ResetDiceAfterFinishedRound();

	UPROPERTY(Transient)
	TArray<int32> TempScore;

	UPROPERTY(Transient)
	TArray<ACPP_Dice*> RegisteredDice;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> DiceScoringRules = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LevelGoalsTable = nullptr;

	UPROPERTY(Transient)
	FDiceRollScoreResult LastSelectionScoreResult;

	UPROPERTY(Transient)
	bool bIsCurrentSelectionValid = false;

	UPROPERTY(Transient)
	int32 CurrentLevelNumber = 1;

	UPROPERTY(Transient)
	int32 CurrentLevelTargetScore = 1500;

	UPROPERTY(Transient)
	bool bLevelWon = false;
	
	UPROPERTY(Transient)
	UDataTable* ScoreDataTable = nullptr;
};
