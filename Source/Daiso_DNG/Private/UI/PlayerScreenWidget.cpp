// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Subsystems/GameManagerSubsystem.h"

/** Передаёт покупку подсистеме, чтобы UI никогда не хранил отдельную копию экономики. */
bool UPlayerScreenWidget::PurchaseStoreBoost(const FName BoostId)
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		return Manager->PurchaseBoost(BoostId);
	}
	return false;
}

/** Закрывает магазин через центральное состояние забега и возвращает результат операции. */
bool UPlayerScreenWidget::CloseRunStore()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		return Manager->CloseStore();
	}
	return false;
}

/** Возвращает свежий список предложений непосредственно из GameManagerSubsystem. */
TArray<FBoostStoreOffer> UPlayerScreenWidget::GetRunStoreOffers() const
{
	if (const UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		return Manager->GetStoreOffers();
	}
	return {};
}

// Создаёт runtime-элементы, привязывает события подсистемы и выполняет первое обновление UI.
void UPlayerScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildRuntimeInterface();

	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->OnDiceSelectionChanged.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleDiceSelectionChanged);
		Manager->OnLevelProgressChanged.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleLevelProgressChanged);
		Manager->OnMoneyChanged.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleMoneyChanged);
		Manager->OnStoreOpened.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleStoreOpened);
		Manager->OnStoreOffersChanged.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleStoreOffersChanged);
		Manager->OnBoostPurchased.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleBoostPurchased);
		Manager->OnStoreClosed.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleStoreClosed);
		Manager->OnGameOver.AddUniqueDynamic(
			this, &UPlayerScreenWidget::HandleGameOver);
	}
	if (IsValid(FinishRoundButton))
	{
		FinishRoundButton->OnClicked.AddUniqueDynamic(this, &UPlayerScreenWidget::HandleFinishRoundClicked);
	}
	RefreshInterface();
}

// Удаляет подписки на подсистему, чтобы экземпляр виджета не удерживался после закрытия уровня.
void UPlayerScreenWidget::NativeDestruct()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->OnDiceSelectionChanged.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleDiceSelectionChanged);
		Manager->OnLevelProgressChanged.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleLevelProgressChanged);
		Manager->OnMoneyChanged.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleMoneyChanged);
		Manager->OnStoreOpened.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleStoreOpened);
		Manager->OnStoreOffersChanged.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleStoreOffersChanged);
		Manager->OnBoostPurchased.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleBoostPurchased);
		Manager->OnStoreClosed.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleStoreClosed);
		Manager->OnGameOver.RemoveDynamic(
			this, &UPlayerScreenWidget::HandleGameOver);
	}
	Super::NativeDestruct();
}

// Добавляет блок прогресса в корневой Canvas и располагает новую кнопку возле GenerateBTN.
void UPlayerScreenWidget::BuildRuntimeInterface()
{
	if (!WidgetTree || IsValid(ProgressPanel))
	{
		return;
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!IsValid(RootPanel))
	{
		UE_LOG(LogTemp, Warning, TEXT("W_PlayerScreen needs a panel root to add level UI."));
		return;
	}

	ProgressPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LevelProgressPanel"));
	ProgressPanel->SetPadding(FMargin(16.0f, 12.0f));
	ProgressPanel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.88f));
	UVerticalBox* ProgressBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("LevelProgressBox"));
	ProgressPanel->SetContent(ProgressBox);

	LevelText = AddProgressText(ProgressBox, TEXT("LevelGoalText"), 18, FLinearColor(0.65f, 0.82f, 1.0f));
	ScoreText = AddProgressText(ProgressBox, TEXT("CurrentScoreText"), 26, FLinearColor::White);
	MoneyText = AddProgressText(ProgressBox, TEXT("RunMoneyText"), 20, FLinearColor(0.95f, 0.82f, 0.25f));
	SelectedDiceText = AddProgressText(ProgressBox, TEXT("SelectedDiceText"), 17, FLinearColor(0.88f, 0.90f, 0.95f));
	VictoryText = AddProgressText(ProgressBox, TEXT("LevelVictoryText"), 20, FLinearColor(1.0f, 0.78f, 0.18f));
	VictoryText->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(RootPanel))
	{
		UCanvasPanelSlot* ProgressSlot = RootCanvas->AddChildToCanvas(ProgressPanel);
		ProgressSlot->SetPosition(ProgressPanelPosition);
		ProgressSlot->SetSize(ProgressPanelSize);
		ProgressSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		ProgressSlot->SetAlignment(FVector2D::ZeroVector);
	}
	else
	{
		RootPanel->AddChild(ProgressPanel);
	}

	GenerateButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("GenerateBTN")));
	FinishRoundButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("FinishRoundButton"));
	FinishRoundButton->SetBackgroundColor(FLinearColor(0.16f, 0.42f, 0.25f, 1.0f));
	FinishRoundButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FinishRoundButtonText"));
	FinishRoundButtonText->SetText(FText::FromString(TEXT("Завершить раунд")));
	FinishRoundButtonText->SetJustification(ETextJustify::Center);
	FSlateFontInfo ButtonFont = FinishRoundButtonText->GetFont();
	ButtonFont.Size = 17;
	FinishRoundButtonText->SetFont(ButtonFont);
	FinishRoundButton->SetContent(FinishRoundButtonText);

	UCanvasPanelSlot* GenerateSlot = IsValid(GenerateButton)
		? Cast<UCanvasPanelSlot>(GenerateButton->Slot)
		: nullptr;
	UCanvasPanel* ButtonCanvas = IsValid(GenerateButton)
		? Cast<UCanvasPanel>(GenerateButton->GetParent())
		: Cast<UCanvasPanel>(RootPanel);
	if (IsValid(ButtonCanvas))
	{
		UCanvasPanelSlot* FinishSlot = ButtonCanvas->AddChildToCanvas(FinishRoundButton);
		if (IsValid(GenerateSlot))
		{
			FinishSlot->SetPosition(GenerateSlot->GetPosition() + FinishRoundButtonOffset);
			FinishSlot->SetAnchors(GenerateSlot->GetAnchors());
			FinishSlot->SetAlignment(GenerateSlot->GetAlignment());
		}
		else
		{
			FinishSlot->SetPosition(FVector2D(240.0f, 640.0f));
			FinishSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			FinishSlot->SetAlignment(FVector2D::ZeroVector);
		}
		FinishSlot->SetSize(FinishRoundButtonSize);
	}
	else if (IsValid(GenerateButton))
	{
		if (UHorizontalBox* ButtonRow = Cast<UHorizontalBox>(GenerateButton->GetParent()))
		{
			UHorizontalBoxSlot* FinishSlot = ButtonRow->AddChildToHorizontalBox(FinishRoundButton);
			FinishSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
		}
		else
		{
			RootPanel->AddChild(FinishRoundButton);
		}
	}
	else
	{
		RootPanel->AddChild(FinishRoundButton);
	}
}

// Конструирует TextBlock с единым шрифтом, цветом и вертикальным интервалом.
UTextBlock* UPlayerScreenWidget::AddProgressText(
	UVerticalBox* Parent, const FName WidgetName, const int32 FontSize, const FLinearColor Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	if (UVerticalBoxSlot* TextSlot = Parent->AddChildToVerticalBox(Text))
	{
		TextSlot->SetPadding(FMargin(0.0f, 1.0f));
	}
	return Text;
}

// Формирует отображаемые строки из состояния подсистемы и выбранных значений костей.
void UPlayerScreenWidget::RefreshInterface()
{
	UGameManagerSubsystem* Manager = ResolveGameManager();
	if (!IsValid(Manager) || !IsValid(LevelText))
	{
		return;
	}

	const FLevelProgressState Progress = Manager->GetLevelProgress();
	LevelText->SetText(FText::FromString(FString::Printf(
		TEXT("Уровень %d  •  Цель: %d"), Progress.LevelNumber, Progress.TargetScore)));
	ScoreText->SetText(FText::FromString(FString::Printf(
		TEXT("Счёт: %d / %d"), Progress.CurrentScore, Progress.TargetScore)));
	MoneyText->SetText(FText::FromString(FString::Printf(TEXT("Монеты: %d"), Progress.Money)));

	TArray<FString> DiceParts;
	for (const int32 DiceValue : Manager->GetSelectedDiceValues())
	{
		DiceParts.Add(FString::FromInt(DiceValue));
	}
	SelectedDiceText->SetText(FText::FromString(FString::Printf(
		TEXT("Выбрано: %s"), DiceParts.IsEmpty() ? TEXT("—") : *FString::Join(DiceParts, TEXT("  •  ")))));

	if (Progress.bGameOver)
	{
		VictoryText->SetText(FText::FromString(TEXT("ЗАБЕГ ПРОИГРАН")));
	}
	else if (Progress.bInStore)
	{
		VictoryText->SetText(FText::FromString(
			Progress.bLastRoundWon ? TEXT("ПОБЕДА • МАГАЗИН") : TEXT("ПОРАЖЕНИЕ • МАГАЗИН")));
	}
	else
	{
		VictoryText->SetText(FText::FromString(TEXT("ЦЕЛЬ ДОСТИГНУТА!")));
	}
	VictoryText->SetVisibility(
		Progress.bLevelWon || Progress.bInStore || Progress.bGameOver
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (IsValid(FinishRoundButton))
	{
		FinishRoundButton->SetIsEnabled(Progress.bCanFinishRound);
	}
	if (IsValid(FinishRoundButtonText))
	{
		FinishRoundButtonText->SetText(FText::FromString(TEXT("Завершить раунд")));
	}
	if (IsValid(GenerateButton))
	{
		GenerateButton->SetIsEnabled(!Progress.bLevelWon && !Progress.bInStore && !Progress.bGameOver);
	}
}

// Получает World Subsystem, содержащий выбор кубиков и прогресс уровня.
UGameManagerSubsystem* UPlayerScreenWidget::ResolveGameManager() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UGameManagerSubsystem>() : nullptr;
}

// Обновляет экран сразу после выбора или отмены выбора кубика.
void UPlayerScreenWidget::HandleDiceSelectionChanged(FDiceRollScoreResult)
{
	RefreshInterface();
}

// Обновляет экран после изменения счёта, смены цели или завершения уровня.
void UPlayerScreenWidget::HandleLevelProgressChanged(FLevelProgressState)
{
	RefreshInterface();
}

/** Обновляет нативный текст денег и сообщает Blueprint точное изменение баланса. */
void UPlayerScreenWidget::HandleMoneyChanged(const int32 NewBalance, const int32 Delta)
{
	BP_OnRunMoneyChanged(NewBalance, Delta);
	RefreshInterface();
}

/** Оповещает Blueprint о готовом магазине и синхронизирует блокировку игровых кнопок. */
void UPlayerScreenWidget::HandleStoreOpened(const FRunProgressState RunState)
{
	BP_OnRunStoreOpened(RunState.StoreOffers);
	RefreshInterface();
}

/** Передаёт Blueprint только актуальные карточки, не заставляя его пересобирать run-state вручную. */
void UPlayerScreenWidget::HandleStoreOffersChanged(const TArray<FBoostStoreOffer>& Offers)
{
	BP_OnRunStoreOffersChanged(Offers);
	RefreshInterface();
}

/** Передаёт успешную покупку и новое число стаков в Blueprint-анимацию интерфейса. */
void UPlayerScreenWidget::HandleBoostPurchased(
	const FBoostStoreOffer PurchasedOffer, const int32 NewStackCount)
{
	BP_OnRunBoostPurchased(PurchasedOffer, NewStackCount);
	RefreshInterface();
}

/** Уведомляет Blueprint о закрытии и показывает уже загруженную следующую либо повторную цель. */
void UPlayerScreenWidget::HandleStoreClosed(FRunProgressState)
{
	BP_OnRunStoreClosed();
	RefreshInterface();
}

/** Передаёт финальный снимок проигранного забега и обновляет нативный статус. */
void UPlayerScreenWidget::HandleGameOver(const FRunProgressState RunState)
{
	BP_OnRunGameOver(RunState);
	RefreshInterface();
}

// Просит подсистему завершить раунд и проверить его результат относительно цели.
void UPlayerScreenWidget::HandleFinishRoundClicked()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->FinishRound();
	}
}
