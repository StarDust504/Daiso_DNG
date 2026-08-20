// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Progression/LevelProgressTypes.h"
#include "RunStoreWidget.generated.h"

class UBorder;
class UButton;
class UGameManagerSubsystem;
class UHorizontalBox;
class UTextBlock;

/**
 * Полностью нативный fallback-магазин для обязательной фазы между раундами.
 * Он не зависит от реализации Blueprint-событий W_PlayerScreen и поэтому одинаково
 * работает в основной игре и в отдельных сценах броска.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API URunStoreWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	static constexpr int32 MaximumVisibleOffers = 4;

	void BuildInterface();
	void RefreshInterface();
	void PurchaseOffer(int32 OfferIndex);
	UGameManagerSubsystem* ResolveGameManager() const;
	FLinearColor GetRarityColor(EBoostRarity Rarity) const;
	FText GetRarityLabel(EBoostRarity Rarity) const;

	UFUNCTION()
	void HandleStoreOpened(FRunProgressState RunState);

	UFUNCTION()
	void HandleStoreOffersChanged(const TArray<FBoostStoreOffer>& Offers);

	UFUNCTION()
	void HandleMoneyChanged(int32 NewBalance, int32 Delta);

	UFUNCTION()
	void HandleGameOver(FRunProgressState RunState);

	UFUNCTION()
	void HandleStoreClosed(FRunProgressState RunState);

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleOffer0Clicked();

	UFUNCTION()
	void HandleOffer1Clicked();

	UFUNCTION()
	void HandleOffer2Clicked();

	UFUNCTION()
	void HandleOffer3Clicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutcomeText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ContinueButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinueButtonText = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OfferCards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferDescriptionTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferMetaTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> OfferButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferButtonTexts;
};
