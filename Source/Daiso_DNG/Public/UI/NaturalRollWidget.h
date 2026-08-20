// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RollOnlyWidget.h"
#include "NaturalRollWidget.generated.h"

class ANaturalDiceRollManager;
class AMouseGatherDiceManager;
class UButton;
class UTextBlock;

/** Adds an independent, physics-decided roll button next to the existing guided roll button. */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API UNaturalRollWidget : public URollOnlyWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildNaturalRollControls();
	ANaturalDiceRollManager* FindNaturalRollManager() const;
	AMouseGatherDiceManager* FindMouseGatherManager() const;
	bool IsAnyGuidedRollActive() const;

	UFUNCTION()
	void HandleNaturalRollClicked();

	UFUNCTION()
	void HandleNaturalRollStarted();

	UFUNCTION()
	void HandleNaturalRollFinished(const TArray<int32>& Results);

	UFUNCTION()
	void HandleNaturalGatherClicked();

	UFUNCTION()
	void HandlePredictedGatherClicked();

	UFUNCTION()
	void HandleGatherStatusChanged(FText Status);

	UPROPERTY(Transient)
	TObjectPtr<UButton> NaturalRollButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NaturalRollButtonText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NaturalRollStatusText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NaturalGatherButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PredictedGatherButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> GuidedRollControl = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ANaturalDiceRollManager> RollManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AMouseGatherDiceManager> GatherManager = nullptr;
};
