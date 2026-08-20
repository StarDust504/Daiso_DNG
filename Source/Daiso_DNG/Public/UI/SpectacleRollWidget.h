// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpectacleRollWidget.generated.h"

class ASpectacleDiceRollManager;
class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;

/** Native overlay containing the theatrical throw controls used only by the isolated dice map. */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API USpectacleRollWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildInterface();
	UButton* AddModeButton(UCanvasPanel* Canvas, FName Name, const FText& Label,
		const FText& Tooltip, const FLinearColor& Color, float Y, bool bAnchorLeft = false);
	ASpectacleDiceRollManager* FindManager() const;
	bool IsOtherDiceInteractionActive() const;
	void BindManager();

	UFUNCTION()
	void HandleVortexNaturalClicked();

	UFUNCTION()
	void HandleVortexPredictedClicked();

	UFUNCTION()
	void HandleMeteorsClicked();

	UFUNCTION()
	void HandleGravityFlipClicked();

	UFUNCTION()
	void HandleHandfulClicked();

	UFUNCTION()
	void HandleBackboardNaturalClicked();

	UFUNCTION()
	void HandleBackboardPredictedClicked();

	UFUNCTION()
	void HandleBackboardDirectedClicked();

	UFUNCTION()
	void HandleStatusChanged(FText Status);

	UPROPERTY(Transient)
	TObjectPtr<ASpectacleDiceRollManager> Manager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> InteractionBlocker = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ModeButtons;
};
