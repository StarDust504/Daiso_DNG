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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRunMoneyChangedSignature, int32, NewBalance, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FStoreOpenedSignature, FRunProgressState, RunState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FStoreOffersChangedSignature, const TArray<FBoostStoreOffer>&, Offers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FBoostPurchasedSignature, FBoostStoreOffer, PurchasedOffer, int32, NewStackCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FStoreClosedSignature, FRunProgressState, RunState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRunGameOverSignature, FRunProgressState, RunState);

/**
 * Единственный владелец состояния текущего забега. Подсистема сохраняет существующий
 * выбор костей и scoring, а progression, магазин и бусты применяет поверх его результата.
 */
UCLASS()
class DAISO_DNG_API UGameManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Загружает scoring/progression Data Tables и начинает новый забег с 25 монетами. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Добавляет значение выбранного кубика, обновляет счёт и возвращает совместимый ключ выбора. */
	UFUNCTION(BlueprintCallable, Category="Score")
	FName AddComboToTempArray(int32 NumberToAppend);

	/** Удаляет одно выбранное значение, обновляет счёт и возвращает совместимый ключ выбора. */
	UFUNCTION(BlueprintCallable, Category="Score")
	FName RemoveComboFromTempArray(int32 NumberToRemove);

	/** Возвращает текущий рассчитанный счёт; Combo сохранён для совместимости старых Blueprint-графов. */
	UFUNCTION(BlueprintCallable, Category="Score")
	int32 GetCurrentScore(FName Combo);

	/** Возвращает отсортированные значения выбранных кубиков. */
	UFUNCTION(BlueprintPure, Category="Score")
	TArray<int32> GetSelectedDiceValues() const;

	/** Возвращает полный результат scoring текущего выбора уже после применимых бустов. */
	UFUNCTION(BlueprintPure, Category="Score")
	FDiceRollScoreResult GetSelectedDiceScore() const;

	/** Возвращает true, если все выбранные кубики входят в результативные комбинации. */
	UFUNCTION(BlueprintPure, Category="Score")
	bool IsCurrentDiceSelectionValid() const;

	/** Полностью очищает выбор кубиков и публикует обновлённый результат. */
	UFUNCTION(BlueprintCallable, Category="Score")
	void ClearDiceSelection();

	/** Вызывается при каждом изменении выбранных костей и рассчитанного результата. */
	UPROPERTY(BlueprintAssignable, Category="Score")
	FDiceSelectionChangedSignature OnDiceSelectionChanged;

	/** Вызывается при изменении счёта, цели, денег или фазы забега. */
	UPROPERTY(BlueprintAssignable, Category="Level Progress")
	FLevelProgressChangedSignature OnLevelProgressChanged;

	/** Вызывается после успешного завершения раунда до открытия следующей цели. */
	UPROPERTY(BlueprintAssignable, Category="Level Progress")
	FLevelWonSignature OnLevelWon;

	/** Вызывается при любом изменении баланса, включая награду, штраф и покупку. */
	UPROPERTY(BlueprintAssignable, Category="Run Progress|Events")
	FRunMoneyChangedSignature OnMoneyChanged;

	/** Вызывается после генерации предложений магазина для завершённого раунда. */
	UPROPERTY(BlueprintAssignable, Category="Run Progress|Events")
	FStoreOpenedSignature OnStoreOpened;

	/** Вызывается, когда покупка или баланс меняют доступность предложений. */
	UPROPERTY(BlueprintAssignable, Category="Run Progress|Events")
	FStoreOffersChangedSignature OnStoreOffersChanged;

	/** Вызывается после успешного списания денег и добавления стака купленного буста. */
	UPROPERTY(BlueprintAssignable, Category="Run Progress|Events")
	FBoostPurchasedSignature OnBoostPurchased;

	/** Вызывается после закрытия магазина и подготовки следующего игрового раунда. */
	UPROPERTY(BlueprintAssignable, Category="Run Progress|Events")
	FStoreClosedSignature OnStoreClosed;

	/** Вызывается один раз, когда баланс после завершения раунда становится не больше нуля. */
	UPROPERTY(BlueprintAssignable, Category="Run Progress|Events")
	FRunGameOverSignature OnGameOver;

	/** Возвращает совместимый снимок счёта, цели, денег и текущей фазы. */
	UFUNCTION(BlueprintPure, Category="Level Progress")
	FLevelProgressState GetLevelProgress() const;

	/** Возвращает полный снимок забега с купленными бустами и предложениями магазина. */
	UFUNCTION(BlueprintPure, Category="Run Progress")
	FRunProgressState GetRunProgress() const;

	/** Возвращает текущий баланс игрока. */
	UFUNCTION(BlueprintPure, Category="Run Progress|Economy")
	int32 GetMoney() const { return Money; }

	/** Возвращает копию всех купленных бустов и их текущих стаков. */
	UFUNCTION(BlueprintPure, Category="Run Progress|Boosts")
	TArray<FOwnedBoostStack> GetOwnedBoosts() const { return OwnedBoosts; }

	/** Возвращает число купленных стаков конкретного буста или ноль, если буст не куплен. */
	UFUNCTION(BlueprintPure, Category="Run Progress|Boosts")
	int32 GetBoostStackCount(FName BoostId) const;

	/** Возвращает подготовленные карточки текущего магазина в стабильном порядке. */
	UFUNCTION(BlueprintPure, Category="Run Progress|Store")
	TArray<FBoostStoreOffer> GetStoreOffers() const { return CurrentStoreOffers; }

	/** Возвращает true только во время обязательной магазинной фазы между раундами. */
	UFUNCTION(BlueprintPure, Category="Run Progress|Store")
	bool IsStoreOpen() const { return bStoreOpen; }

	/** Возвращает true после исчерпания капитала; новый игровой раунд тогда уже не запускается. */
	UFUNCTION(BlueprintPure, Category="Run Progress")
	bool IsGameOver() const { return bGameOver; }

	/**
	 * Завершает валидный игровой раунд, сравнивает счёт с RequiredInvoice, применяет
	 * Win/Lose и всегда открывает магазин; при Money <= 0 дополнительно завершает забег.
	 */
	UFUNCTION(BlueprintCallable, Category="Run Progress")
	bool FinishRound();

	/**
	 * Покупает выбранное активное предложение, если хватает денег и MaxStacks не достигнут;
	 * при успехе предложение помечается использованным до следующего магазина.
	 */
	UFUNCTION(BlueprintCallable, Category="Run Progress|Store")
	bool PurchaseBoost(FName BoostId);

	/**
	 * Закрывает текущий магазин; после победы загружает следующую цель, после поражения
	 * повторяет текущую, а при Game Over только закрывает интерфейс без запуска раунда.
	 */
	UFUNCTION(BlueprintCallable, Category="Run Progress|Store")
	bool CloseStore();

	/** Полностью начинает новый забег, сбрасывая деньги, магазин, стаки и игровой счёт. */
	UFUNCTION(BlueprintCallable, Category="Run Progress")
	void ResetRun();

	/**
	 * Применяет купленные бусты к переданному scoring-контексту в порядке Base, +Mult,
	 * XMult и специальных Farkle-операций; функция не изменяет состояние забега.
	 */
	UFUNCTION(BlueprintPure, Category="Run Progress|Boosts")
	FBoostEffectResult EvaluateBoostEffects(const FBoostEffectContext& Context) const;

	/** Переключает текущий раунд на строку DT_RunStages и очищает только игровой счёт. */
	UFUNCTION(BlueprintCallable, Category="Level Progress")
	bool SetCurrentLevelNumber(int32 NewLevelNumber);

	/** Назначает progression-таблицы и перезапускает забег; полезно для прототипов и тестовых наборов данных. */
	UFUNCTION(BlueprintCallable, Category="Data")
	void RegisterRunDataTables(UDataTable* RunStagesDT, UDataTable* BoostsDT);

	/** Сохраняет старую таблицу очков, используемую существующими Blueprint-графами. */
	UFUNCTION(BlueprintCallable, Category="Data")
	void RegisterScoreDataTable(UDataTable* ScoreDT);

	/** Регистрирует кубик в подсистеме, если он ещё не зарегистрирован. */
	UFUNCTION(BlueprintCallable, Category="Player")
	void RegisterDice(ACPP_Dice* DiceToRegister);

	/** Удаляет кубик из списка зарегистрированных. */
	UFUNCTION(BlueprintCallable, Category="Player")
	void UnregisterDice(ACPP_Dice* DiceToUnregister);

	/** Проверяет, содержится ли кубик в списке зарегистрированных. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Player")
	bool CheckIsDiceRegistered(ACPP_Dice* DiceToCheck) const;

	/** Сохраняет прежнее Blueprint-поведение скрытия либо освобождения зарегистрированных костей. */
	UFUNCTION(BlueprintCallable, Category="Player")
	void DestroyRegisteredDice();

#if WITH_DEV_AUTOMATION_TESTS
	/** Добавляет один стак через те же проверки MaxStacks без экономики; доступно только Automation Tests. */
	bool AddBoostStackForTests(FName BoostId);

	/** Задаёт seed магазина для полностью воспроизводимых Automation Tests. */
	void SetStoreRandomSeedForTests(int32 Seed);
#endif

private:
	/** Строит совместимый ключ из отсортированных значений выбранных кубиков. */
	FName BuildSelectedDiceKey() const;

	/** Пересчитывает текущий выбор старым scorer, затем накладывает купленные бусты и публикует результат. */
	void RefreshSelectionScore();

	/** Загружает и проверяет строку текущего раунда, используя DT_LevelGoals только как legacy fallback. */
	bool LoadCurrentRunStage();

	/** Находит валидную строку произвольного раунда и копирует её в OutStage. */
	bool TryGetRunStage(int32 RoundNumber, FRunStageRow& OutStage) const;

	/** Находит строку буста по ID и проверяет, что DT_Boosts имеет ожидаемый RowStruct. */
	const FBoostRow* FindBoostRow(FName BoostId) const;

	/** Публикует актуальное совместимое состояние уровня всем подписчикам. */
	void PublishLevelProgress();

	/** Меняет баланс на Delta и рассылает отдельное событие экономики. */
	void ChangeMoney(int32 Delta);

	/** Создаёт уникальные предложения по весам и размеру магазина завершённого раунда. */
	void GenerateStoreOffers(const FRunStageRow& CompletedStage);

	/** Выбирает одну редкость по нормализованным весам строки DT_RunStages. */
	EBoostRarity RollStoreRarity(const FRunStageRow& Stage);

	/** Собирает UI-карточку из строки DT_Boosts и текущего состояния забега. */
	FBoostStoreOffer MakeStoreOffer(FName BoostId, const FBoostRow& Row) const;

	/** Пересчитывает CurrentStacks/bCanPurchase после покупки или изменения баланса. */
	void RefreshStoreOfferAvailability();

	/** Увеличивает существующий стак либо добавляет новый, строго соблюдая MaxStacks. */
	bool AddBoostStack(FName BoostId, const FBoostRow& Row, int32& OutNewStackCount);

	/** Возвращает true, если конкретный триггер строки активен в переданном контексте. */
	bool DoesEffectTriggerMatch(const FBoostRow& Row, const FBoostEffectContext& Context) const;

	/** Снимает выбор и возвращает зарегистрированные кубики в состояние нового раунда. */
	void ResetDiceAfterFinishedRound();

	/** Очищает только scoring-состояние, не затрагивая деньги, магазин и купленные бусты. */
	void ResetRoundScore();

	UPROPERTY(Transient)
	TArray<int32> TempScore;

	UPROPERTY(Transient)
	TArray<ACPP_Dice*> RegisteredDice;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> DiceScoringRules = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LevelGoalsTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> RunStagesTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BoostsTable = nullptr;

	UPROPERTY(Transient)
	FDiceRollScoreResult LastSelectionScoreResult;

	UPROPERTY(Transient)
	bool bIsCurrentSelectionValid = false;

	UPROPERTY(Transient)
	int32 CurrentLevelNumber = 1;

	UPROPERTY(Transient)
	int32 CurrentLevelTargetScore = 1500;

	UPROPERTY(Transient)
	FRunStageRow CurrentRunStage;

	UPROPERTY(Transient)
	int32 PendingLevelNumber = 1;

	UPROPERTY(Transient)
	int32 Money = 25;

	UPROPERTY(Transient)
	bool bLevelWon = false;

	UPROPERTY(Transient)
	bool bLastRoundWon = false;

	UPROPERTY(Transient)
	bool bStoreOpen = false;

	UPROPERTY(Transient)
	bool bGameOver = false;

	UPROPERTY(Transient)
	TArray<FOwnedBoostStack> OwnedBoosts;

	UPROPERTY(Transient)
	TArray<FBoostStoreOffer> CurrentStoreOffers;

	FRandomStream StoreRandomStream;

	UPROPERTY(Transient)
	UDataTable* ScoreDataTable = nullptr;
};
