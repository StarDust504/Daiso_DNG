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

	TArray<FString> DiceParts;
	for (const int32 DiceValue : Manager->GetSelectedDiceValues())
	{
		DiceParts.Add(FString::FromInt(DiceValue));
	}
	SelectedDiceText->SetText(FText::FromString(FString::Printf(
		TEXT("Выбрано: %s"), DiceParts.IsEmpty() ? TEXT("—") : *FString::Join(DiceParts, TEXT("  •  ")))));

	VictoryText->SetText(FText::FromString(TEXT("ЦЕЛЬ ДОСТИГНУТА!")));
	VictoryText->SetVisibility(Progress.bLevelWon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
		GenerateButton->SetIsEnabled(!Progress.bLevelWon);
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

// Просит подсистему завершить раунд и проверить его результат относительно цели.
void UPlayerScreenWidget::HandleFinishRoundClicked()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->FinishRound();
	}
}
