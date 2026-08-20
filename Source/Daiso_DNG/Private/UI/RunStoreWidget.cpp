// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RunStoreWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Subsystems/GameManagerSubsystem.h"

namespace RunStoreStyle
{
	static UTextBlock* MakeText(UWidgetTree& Tree, const FName Name, const int32 Size,
		const FLinearColor Color, const ETextJustify::Type Justification = ETextJustify::Center)
	{
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		return Text;
	}
}

void URunStoreWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildInterface();
}

void URunStoreWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildInterface();
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->OnStoreOpened.AddUniqueDynamic(this, &URunStoreWidget::HandleStoreOpened);
		Manager->OnStoreOffersChanged.AddUniqueDynamic(this, &URunStoreWidget::HandleStoreOffersChanged);
		Manager->OnMoneyChanged.AddUniqueDynamic(this, &URunStoreWidget::HandleMoneyChanged);
		Manager->OnGameOver.AddUniqueDynamic(this, &URunStoreWidget::HandleGameOver);
		Manager->OnStoreClosed.AddUniqueDynamic(this, &URunStoreWidget::HandleStoreClosed);
	}
	RefreshInterface();
}

void URunStoreWidget::NativeDestruct()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->OnStoreOpened.RemoveDynamic(this, &URunStoreWidget::HandleStoreOpened);
		Manager->OnStoreOffersChanged.RemoveDynamic(this, &URunStoreWidget::HandleStoreOffersChanged);
		Manager->OnMoneyChanged.RemoveDynamic(this, &URunStoreWidget::HandleMoneyChanged);
		Manager->OnGameOver.RemoveDynamic(this, &URunStoreWidget::HandleGameOver);
		Manager->OnStoreClosed.RemoveDynamic(this, &URunStoreWidget::HandleStoreClosed);
	}
	Super::NativeDestruct();
}

/** Строит затемняющий полноэкранный слой, четыре карточки и кнопку перехода. */
void URunStoreWidget::BuildInterface()
{
	if (!WidgetTree || IsValid(OutcomeText))
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("RunStoreRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunStoreDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.024f, 0.91f));
	UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(Dimmer);
	DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimmerSlot->SetOffsets(FMargin(0.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunStorePanel"));
	Panel->SetBrushColor(FLinearColor(0.035f, 0.052f, 0.082f, 0.98f));
	Panel->SetPadding(FMargin(30.0f, 24.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(1160.0f, 600.0f));
	PanelSlot->SetZOrder(1);

	UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("RunStoreMainBox"));
	Panel->SetContent(MainBox);

	OutcomeText = RunStoreStyle::MakeText(*WidgetTree, TEXT("RunStoreOutcome"), 34,
		FLinearColor(0.92f, 0.96f, 1.0f, 1.0f));
	MainBox->AddChildToVerticalBox(OutcomeText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	MoneyText = RunStoreStyle::MakeText(*WidgetTree, TEXT("RunStoreMoney"), 22,
		FLinearColor(1.0f, 0.82f, 0.25f, 1.0f));
	MainBox->AddChildToVerticalBox(MoneyText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	UHorizontalBox* OffersBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("RunStoreOffers"));
	UVerticalBoxSlot* OffersRowSlot = MainBox->AddChildToVerticalBox(OffersBox);
	OffersRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	for (int32 Index = 0; Index < MaximumVisibleOffers; ++Index)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			FName(*FString::Printf(TEXT("RunStoreOfferCard%d"), Index)));
		Card->SetPadding(FMargin(15.0f));
		UHorizontalBoxSlot* CardSlot = OffersBox->AddChildToHorizontalBox(Card);
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetPadding(FMargin(7.0f, 0.0f));

		UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			FName(*FString::Printf(TEXT("RunStoreOfferBox%d"), Index)));
		Card->SetContent(CardBox);

		UTextBlock* Name = RunStoreStyle::MakeText(*WidgetTree,
			FName(*FString::Printf(TEXT("RunStoreOfferName%d"), Index)), 21, FLinearColor::White);
		CardBox->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		UTextBlock* Description = RunStoreStyle::MakeText(*WidgetTree,
			FName(*FString::Printf(TEXT("RunStoreOfferDescription%d"), Index)), 16,
			FLinearColor(0.87f, 0.90f, 0.96f, 1.0f));
		UVerticalBoxSlot* DescriptionSlot = CardBox->AddChildToVerticalBox(Description);
		DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DescriptionSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 8.0f));
		UTextBlock* Meta = RunStoreStyle::MakeText(*WidgetTree,
			FName(*FString::Printf(TEXT("RunStoreOfferMeta%d"), Index)), 15,
			FLinearColor(0.98f, 0.83f, 0.40f, 1.0f));
		CardBox->AddChildToVerticalBox(Meta)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 9.0f));

		UButton* BuyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
			FName(*FString::Printf(TEXT("RunStoreOfferButton%d"), Index)));
		BuyButton->SetBackgroundColor(FLinearColor(0.16f, 0.43f, 0.27f, 1.0f));
		UTextBlock* BuyText = RunStoreStyle::MakeText(*WidgetTree,
			FName(*FString::Printf(TEXT("RunStoreOfferButtonText%d"), Index)), 17, FLinearColor::White);
		BuyButton->SetContent(BuyText);
		CardBox->AddChildToVerticalBox(BuyButton)->SetPadding(FMargin(5.0f, 2.0f));

		OfferCards.Add(Card);
		OfferNameTexts.Add(Name);
		OfferDescriptionTexts.Add(Description);
		OfferMetaTexts.Add(Meta);
		OfferButtons.Add(BuyButton);
		OfferButtonTexts.Add(BuyText);
	}
	OfferButtons[0]->OnClicked.AddUniqueDynamic(this, &URunStoreWidget::HandleOffer0Clicked);
	OfferButtons[1]->OnClicked.AddUniqueDynamic(this, &URunStoreWidget::HandleOffer1Clicked);
	OfferButtons[2]->OnClicked.AddUniqueDynamic(this, &URunStoreWidget::HandleOffer2Clicked);
	OfferButtons[3]->OnClicked.AddUniqueDynamic(this, &URunStoreWidget::HandleOffer3Clicked);

	StatusText = RunStoreStyle::MakeText(*WidgetTree, TEXT("RunStoreStatus"), 16,
		FLinearColor(0.72f, 0.82f, 0.95f, 1.0f));
	MainBox->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 8.0f));

	ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RunStoreContinueButton"));
	ContinueButton->SetBackgroundColor(FLinearColor(0.12f, 0.31f, 0.56f, 1.0f));
	ContinueButton->OnClicked.AddUniqueDynamic(this, &URunStoreWidget::HandleContinueClicked);
	ContinueButtonText = RunStoreStyle::MakeText(*WidgetTree, TEXT("RunStoreContinueText"), 21,
		FLinearColor::White);
	ContinueButton->SetContent(ContinueButtonText);
	MainBox->AddChildToVerticalBox(ContinueButton)->SetPadding(FMargin(360.0f, 0.0f));
	SetVisibility(ESlateVisibility::Collapsed);
}

void URunStoreWidget::RefreshInterface()
{
	UGameManagerSubsystem* Manager = ResolveGameManager();
	if (!IsValid(Manager) || !IsValid(OutcomeText))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const FRunProgressState State = Manager->GetRunProgress();
	SetVisibility(State.bStoreOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!State.bStoreOpen)
	{
		return;
	}

	OutcomeText->SetText(State.bGameOver
		? NSLOCTEXT("RunStoreWidget", "GameOverTitle", "ЗАБЕГ ЗАВЕРШЁН")
		: State.bLastRoundWon
			? NSLOCTEXT("RunStoreWidget", "WinTitle", "ПОБЕДА — МАГАЗИН УЛУЧШЕНИЙ")
			: NSLOCTEXT("RunStoreWidget", "LoseTitle", "РАУНД НЕ ПРОЙДЕН — МАГАЗИН"));
	MoneyText->SetText(FText::Format(NSLOCTEXT("RunStoreWidget", "Money", "Монеты: {0}"),
		FText::AsNumber(State.Money)));
	ContinueButtonText->SetText(State.bGameOver
		? NSLOCTEXT("RunStoreWidget", "Restart", "Начать новый забег")
		: NSLOCTEXT("RunStoreWidget", "Continue", "Продолжить"));

	for (int32 Index = 0; Index < MaximumVisibleOffers; ++Index)
	{
		const bool bHasOffer = State.StoreOffers.IsValidIndex(Index);
		OfferCards[Index]->SetVisibility(bHasOffer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bHasOffer)
		{
			continue;
		}
		const FBoostStoreOffer& Offer = State.StoreOffers[Index];
		OfferCards[Index]->SetBrushColor(GetRarityColor(Offer.Rarity));
		OfferNameTexts[Index]->SetText(Offer.DisplayName);
		OfferDescriptionTexts[Index]->SetText(Offer.EffectDescription);
		OfferMetaTexts[Index]->SetText(FText::Format(
			NSLOCTEXT("RunStoreWidget", "OfferMeta", "{0}  •  {1} мон.  •  стак {2}/{3}"),
			GetRarityLabel(Offer.Rarity), FText::AsNumber(Offer.Cost),
			FText::AsNumber(Offer.CurrentStacks), FText::AsNumber(Offer.MaxStacks)));
		OfferButtons[Index]->SetIsEnabled(Offer.bCanPurchase && !State.bGameOver);
		OfferButtonTexts[Index]->SetText(Offer.bPurchased
			? NSLOCTEXT("RunStoreWidget", "Purchased", "Куплено")
			: Offer.bCanPurchase
				? NSLOCTEXT("RunStoreWidget", "Buy", "Купить")
				: NSLOCTEXT("RunStoreWidget", "Unavailable", "Недоступно"));
	}
	StatusText->SetText(State.bGameOver
		? NSLOCTEXT("RunStoreWidget", "GameOverHint", "Баланс исчерпан. Можно сразу начать новый забег.")
		: NSLOCTEXT("RunStoreWidget", "StoreHint", "Купленные улучшения сохраняются до конца забега."));
}

void URunStoreWidget::PurchaseOffer(const int32 OfferIndex)
{
	UGameManagerSubsystem* Manager = ResolveGameManager();
	const TArray<FBoostStoreOffer> Offers = IsValid(Manager) ? Manager->GetStoreOffers() : TArray<FBoostStoreOffer>();
	if (!IsValid(Manager) || !Offers.IsValidIndex(OfferIndex)
		|| !Manager->PurchaseBoost(Offers[OfferIndex].BoostId))
	{
		if (IsValid(StatusText))
		{
			StatusText->SetText(NSLOCTEXT("RunStoreWidget", "PurchaseFailed",
				"Покупка недоступна: проверьте цену и число стаков."));
		}
		return;
	}
	StatusText->SetText(NSLOCTEXT("RunStoreWidget", "PurchaseSucceeded", "Улучшение добавлено в забег."));
	RefreshInterface();
}

UGameManagerSubsystem* URunStoreWidget::ResolveGameManager() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UGameManagerSubsystem>() : nullptr;
}

FLinearColor URunStoreWidget::GetRarityColor(const EBoostRarity Rarity) const
{
	switch (Rarity)
	{
	case EBoostRarity::Rare: return FLinearColor(0.055f, 0.19f, 0.42f, 0.98f);
	case EBoostRarity::Epic: return FLinearColor(0.28f, 0.075f, 0.42f, 0.98f);
	case EBoostRarity::Legendary: return FLinearColor(0.47f, 0.22f, 0.025f, 0.98f);
	default: return FLinearColor(0.10f, 0.16f, 0.23f, 0.98f);
	}
}

FText URunStoreWidget::GetRarityLabel(const EBoostRarity Rarity) const
{
	switch (Rarity)
	{
	case EBoostRarity::Rare: return NSLOCTEXT("RunStoreWidget", "Rare", "Редкий");
	case EBoostRarity::Epic: return NSLOCTEXT("RunStoreWidget", "Epic", "Эпический");
	case EBoostRarity::Legendary: return NSLOCTEXT("RunStoreWidget", "Legendary", "Легендарный");
	default: return NSLOCTEXT("RunStoreWidget", "Common", "Обычный");
	}
}

void URunStoreWidget::HandleStoreOpened(FRunProgressState) { RefreshInterface(); }
void URunStoreWidget::HandleStoreOffersChanged(const TArray<FBoostStoreOffer>&) { RefreshInterface(); }
void URunStoreWidget::HandleMoneyChanged(int32, int32) { RefreshInterface(); }
void URunStoreWidget::HandleGameOver(FRunProgressState) { RefreshInterface(); }
void URunStoreWidget::HandleStoreClosed(FRunProgressState) { RefreshInterface(); }

void URunStoreWidget::HandleContinueClicked()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		if (Manager->IsGameOver())
		{
			Manager->ResetRun();
		}
		else
		{
			Manager->CloseStore();
		}
	}
	RefreshInterface();
}

void URunStoreWidget::HandleOffer0Clicked() { PurchaseOffer(0); }
void URunStoreWidget::HandleOffer1Clicked() { PurchaseOffer(1); }
void URunStoreWidget::HandleOffer2Clicked() { PurchaseOffer(2); }
void URunStoreWidget::HandleOffer3Clicked() { PurchaseOffer(3); }
