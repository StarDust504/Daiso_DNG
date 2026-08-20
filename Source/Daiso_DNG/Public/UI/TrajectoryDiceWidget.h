// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dice/DiceScoringTypes.h"
#include "Progression/LevelProgressTypes.h"
#include "TrajectoryDiceWidget.generated.h"

class AMouseGatherDiceManager;
class ASpectacleDiceRollManager;
class ACPP_Dice;
class UBorder;
class UButton;
class UCanvasPanel;
class UGameManagerSubsystem;
class UTextBlock;

/**
 * Игровая панель новой сцены: ровно два способа броска, нижняя панель выбора
 * выпавших граней, scoring и кнопка завершения раунда.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API UTrajectoryDiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Привязывает значения завершённого броска к конкретным физическим кубикам. */
	UFUNCTION(BlueprintCallable, Category="Dice|Trajectory")
	void ApplyRollResults(const TArray<int32>& Results, const TArray<ACPP_Dice*>& DiceActors);

	/** Переключает удержание физического кубика и синхронизирует scoring с нижней панелью. */
	UFUNCTION(BlueprintCallable, Category="Dice|Trajectory")
	bool TogglePhysicalDiceSelection(ACPP_Dice* Dice);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildInterface();
	UButton* AddActionButton(UCanvasPanel* Canvas, FName Name, const FText& Label,
		const FText& Tooltip, const FLinearColor& Color, float X, float Width);
	void ResolveAndBindManagers();
	AMouseGatherDiceManager* FindGatherManager() const;
	ASpectacleDiceRollManager* FindSpectacleManager() const;
	UGameManagerSubsystem* ResolveGameManager() const;
	bool IsAnyRollActive() const;
	void PrepareForReroll();
	void AutoSelectScoringDice(const TArray<int32>& Results, const TArray<ACPP_Dice*>& DiceActors);
	void SubmitSelectedDice();
	void SyncPhysicalSelection();
	bool ToggleResult(int32 Index);
	bool TryToggleDieUnderCursor();
	bool IsPointerOverControls() const;
	void RefreshProgress();
	void RefreshResultStrip();

	UFUNCTION()
	void HandleManualGatherClicked();

	UFUNCTION()
	void HandleAutomaticGatherClicked();

	UFUNCTION()
	void HandleFinishRoundClicked();

	UFUNCTION()
	void HandleGatherStatusChanged(FText Status);

	UFUNCTION()
	void HandleSpectacleStatusChanged(FText Status);

	UFUNCTION()
	void HandleGatherRollFinished(const TArray<int32>& Results);

	UFUNCTION()
	void HandleSpectacleRollFinished(const TArray<int32>& Results);

	UFUNCTION()
	void HandleScoreChanged(FDiceRollScoreResult ScoreResult);

	UFUNCTION()
	void HandleProgressChanged(FLevelProgressState ProgressState);

	UFUNCTION()
	void HandleResult0Clicked();

	UFUNCTION()
	void HandleResult1Clicked();

	UFUNCTION()
	void HandleResult2Clicked();

	UFUNCTION()
	void HandleResult3Clicked();

	UFUNCTION()
	void HandleResult4Clicked();

	UFUNCTION()
	void HandleResult5Clicked();

	UPROPERTY(Transient)
	TObjectPtr<AMouseGatherDiceManager> GatherManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ASpectacleDiceRollManager> SpectacleManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoundText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectionHintText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultsPanel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ManualGatherButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AutomaticGatherButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FinishRoundButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FinishRoundButtonText = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ResultButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ResultButtonTexts;

	TArray<int32> LastRollResults;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACPP_Dice>> LastRollDice;

	TArray<bool> SelectedResults;
	bool bPreviousStoreOpen = false;
	bool bPreviousSelectionClickDown = false;
};
