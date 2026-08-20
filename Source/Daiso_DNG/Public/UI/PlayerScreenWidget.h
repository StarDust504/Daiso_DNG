// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dice/DiceScoringTypes.h"
#include "Progression/LevelProgressTypes.h"
#include "PlayerScreenWidget.generated.h"

class UBorder;
class UButton;
class UGameManagerSubsystem;
class URunStoreWidget;
class UTextBlock;
class UVerticalBox;

/**
 * Нативная основа существующего W_PlayerScreen.
 * Добавляет блок прогресса и кнопку завершения раунда, сохраняя исходную кнопку GenerateBTN.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API UPlayerScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Покупает предложение через единственный run-state в GameManagerSubsystem. */
	UFUNCTION(BlueprintCallable, Category="Run UI|Store")
	bool PurchaseStoreBoost(FName BoostId);

	/** Закрывает магазин через GameManagerSubsystem и запускает разрешённый следующий раунд. */
	UFUNCTION(BlueprintCallable, Category="Run UI|Store")
	bool CloseRunStore();

	/** Возвращает текущие предложения, чтобы Blueprint мог построить собственные карточки магазина. */
	UFUNCTION(BlueprintPure, Category="Run UI|Store")
	TArray<FBoostStoreOffer> GetRunStoreOffers() const;

	/** Blueprint-точка расширения для обновления виджета денег без жёсткой зависимости от его дизайна. */
	UFUNCTION(BlueprintImplementableEvent, Category="Run UI|Events", meta=(DisplayName="Run Money Changed"))
	void BP_OnRunMoneyChanged(int32 NewBalance, int32 Delta);

	/** Blueprint-точка расширения для создания магазина из сгенерированных предложений. */
	UFUNCTION(BlueprintImplementableEvent, Category="Run UI|Events", meta=(DisplayName="Run Store Opened"))
	void BP_OnRunStoreOpened(const TArray<FBoostStoreOffer>& Offers);

	/** Blueprint-точка расширения для обновления доступности карточек после изменения состояния. */
	UFUNCTION(BlueprintImplementableEvent, Category="Run UI|Events", meta=(DisplayName="Run Store Offers Changed"))
	void BP_OnRunStoreOffersChanged(const TArray<FBoostStoreOffer>& Offers);

	/** Blueprint-точка расширения для анимации успешной покупки и нового числа стаков. */
	UFUNCTION(BlueprintImplementableEvent, Category="Run UI|Events", meta=(DisplayName="Run Boost Purchased"))
	void BP_OnRunBoostPurchased(FBoostStoreOffer PurchasedOffer, int32 NewStackCount);

	/** Blueprint-точка расширения для скрытия магазина после подтверждённого закрытия. */
	UFUNCTION(BlueprintImplementableEvent, Category="Run UI|Events", meta=(DisplayName="Run Store Closed"))
	void BP_OnRunStoreClosed();

	/** Blueprint-точка расширения для показа финального состояния проигранного забега. */
	UFUNCTION(BlueprintImplementableEvent, Category="Run UI|Events", meta=(DisplayName="Run Game Over"))
	void BP_OnRunGameOver(FRunProgressState RunState);

	/** Позиция блока счёта на Canvas; меняется в Class Defaults у W_PlayerScreen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Level UI|Layout")
	FVector2D ProgressPanelPosition = FVector2D(40.0f, 40.0f);

	/** Размер блока счёта на Canvas. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Level UI|Layout")
	FVector2D ProgressPanelSize = FVector2D(330.0f, 190.0f);

	/** Смещение кнопки завершения относительно существующей GenerateBTN. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Level UI|Layout")
	FVector2D FinishRoundButtonOffset = FVector2D(0.0f, -70.0f);

	/** Размер кнопки завершения раунда на Canvas. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Level UI|Layout")
	FVector2D FinishRoundButtonSize = FVector2D(180.0f, 52.0f);

protected:
	/** Создаёт недостающие элементы, подписывается на подсистему и показывает актуальное состояние. */
	virtual void NativeConstruct() override;

	/** Снимает динамические подписки перед уничтожением экземпляра виджета. */
	virtual void NativeDestruct() override;

private:
	/** Создаёт блок прогресса и кнопку рядом с GenerateBTN во время выполнения. */
	void BuildRuntimeInterface();

	/** Создаёт одну оформленную строку текста внутри блока прогресса. */
	UTextBlock* AddProgressText(UVerticalBox* Parent, FName WidgetName, int32 FontSize, FLinearColor Color);

	/** Обновляет все подписи, доступность кнопок и сообщение о победе. */
	void RefreshInterface();

	/** Возвращает игровую подсистему текущего мира. */
	UGameManagerSubsystem* ResolveGameManager() const;

	/** Реагирует на изменение набора выбранных кубиков. */
	UFUNCTION()
	void HandleDiceSelectionChanged(FDiceRollScoreResult ScoreResult);

	/** Реагирует на изменение текущего счёта, цели или результата раунда. */
	UFUNCTION()
	void HandleLevelProgressChanged(FLevelProgressState ProgressState);

	/** Передаёт изменение баланса нативной подписи и Blueprint-расширению интерфейса. */
	UFUNCTION()
	void HandleMoneyChanged(int32 NewBalance, int32 Delta);

	/** Передаёт сгенерированный магазин Blueprint-слою и блокирует игровые кнопки. */
	UFUNCTION()
	void HandleStoreOpened(FRunProgressState RunState);

	/** Передаёт актуальный список предложений Blueprint-слою после покупок и смены баланса. */
	UFUNCTION()
	void HandleStoreOffersChanged(const TArray<FBoostStoreOffer>& Offers);

	/** Передаёт успешную покупку Blueprint-слою и обновляет нативные подписи. */
	UFUNCTION()
	void HandleBoostPurchased(FBoostStoreOffer PurchasedOffer, int32 NewStackCount);

	/** Передаёт закрытие магазина Blueprint-слою и возвращает UI к игровому раунду. */
	UFUNCTION()
	void HandleStoreClosed(FRunProgressState RunState);

	/** Передаёт проигрыш забега Blueprint-слою и оставляет игровые кнопки заблокированными. */
	UFUNCTION()
	void HandleGameOver(FRunProgressState RunState);

	/** Передаёт нажатие кнопки в GameManagerSubsystem::FinishRound. */
	UFUNCTION()
	void HandleFinishRoundClicked();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ProgressPanel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedDiceText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VictoryText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FinishRoundButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FinishRoundButtonText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GenerateButton = nullptr;

	/** Нативный магазин-подстраховка для W_PlayerScreen без реализованных Blueprint store-events. */
	UPROPERTY(Transient)
	TObjectPtr<URunStoreWidget> RunStoreWidget = nullptr;
};
