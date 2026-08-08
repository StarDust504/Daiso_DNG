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
	TObjectPtr<UTextBlock> SelectedDiceText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VictoryText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FinishRoundButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FinishRoundButtonText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GenerateButton = nullptr;
};
