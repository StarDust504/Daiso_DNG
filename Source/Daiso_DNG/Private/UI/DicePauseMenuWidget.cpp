// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DicePauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/DiceRollPlayerController.h"

void UDicePauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildInterface();
}

void UDicePauseMenuWidget::BuildInterface()
{
	if (!WidgetTree || IsValid(ContinueButton))
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("PauseMenuRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.012f, 0.018f, 0.035f, 0.80f));
	UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(Dimmer);
	DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimmerSlot->SetOffsets(FMargin(0.0f));
	DimmerSlot->SetZOrder(0);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PausePanel"));
	Panel->SetBrushColor(FLinearColor(0.035f, 0.065f, 0.105f, 0.98f));
	Panel->SetPadding(FMargin(42.0f, 34.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(460.0f, 330.0f));
	PanelSlot->SetZOrder(1);

	UVerticalBox* MenuColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PauseMenuColumn"));
	Panel->SetContent(MenuColumn);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseTitle"));
	Title->SetText(NSLOCTEXT("DicePauseMenu", "Title", "ПАУЗА"));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.93f, 1.0f, 1.0f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 38;
	Title->SetFont(TitleFont);
	UVerticalBoxSlot* TitleSlot = MenuColumn->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Fill);

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PauseSubtitle"));
	Subtitle->SetText(NSLOCTEXT("DicePauseMenu", "Subtitle", "Бросок приостановлен"));
	Subtitle->SetJustification(ETextJustify::Center);
	Subtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.70f, 0.80f, 1.0f)));
	FSlateFontInfo SubtitleFont = Subtitle->GetFont();
	SubtitleFont.Size = 16;
	Subtitle->SetFont(SubtitleFont);
	UVerticalBoxSlot* SubtitleSlot = MenuColumn->AddChildToVerticalBox(Subtitle);
	SubtitleSlot->SetHorizontalAlignment(HAlign_Fill);
	SubtitleSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 24.0f));

	ContinueButton = AddMenuButton(MenuColumn, TEXT("ContinueButton"),
		NSLOCTEXT("DicePauseMenu", "Continue", "Продолжить"),
		FLinearColor(0.04f, 0.42f, 0.58f, 1.0f));
	ContinueButton->OnClicked.AddUniqueDynamic(this, &UDicePauseMenuWidget::HandleContinueClicked);

	USpacer* ButtonGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("ButtonGap"));
	ButtonGap->SetSize(FVector2D(1.0f, 14.0f));
	MenuColumn->AddChildToVerticalBox(ButtonGap);

	QuitButton = AddMenuButton(MenuColumn, TEXT("QuitButton"),
		NSLOCTEXT("DicePauseMenu", "Quit", "Выход из игры"),
		FLinearColor(0.45f, 0.075f, 0.085f, 1.0f));
	QuitButton->OnClicked.AddUniqueDynamic(this, &UDicePauseMenuWidget::HandleQuitClicked);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseHint"));
	Hint->SetText(NSLOCTEXT("DicePauseMenu", "Hint", "ESC — вернуться к игре"));
	Hint->SetJustification(ETextJustify::Center);
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.48f, 0.55f, 0.64f, 1.0f)));
	FSlateFontInfo HintFont = Hint->GetFont();
	HintFont.Size = 13;
	Hint->SetFont(HintFont);
	UVerticalBoxSlot* HintSlot = MenuColumn->AddChildToVerticalBox(Hint);
	HintSlot->SetHorizontalAlignment(HAlign_Fill);
	HintSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
}

UButton* UDicePauseMenuWidget::AddMenuButton(UVerticalBox* Container, const FName Name,
	const FText& Label, const FLinearColor& Color)
{
	USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("Box"))));
	ButtonBox->SetHeightOverride(58.0f);
	UVerticalBoxSlot* BoxSlot = Container->AddChildToVerticalBox(ButtonBox);
	BoxSlot->SetHorizontalAlignment(HAlign_Fill);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(Color);
	ButtonBox->SetContent(Button);

	UTextBlock* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Label"))));
	LabelWidget->SetText(Label);
	LabelWidget->SetJustification(ETextJustify::Center);
	LabelWidget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo LabelFont = LabelWidget->GetFont();
	LabelFont.Size = 21;
	LabelWidget->SetFont(LabelFont);
	Button->SetContent(LabelWidget);
	return Button;
}

void UDicePauseMenuWidget::HandleContinueClicked()
{
	if (ADiceRollPlayerController* Controller = Cast<ADiceRollPlayerController>(GetOwningPlayer()))
	{
		Controller->ClosePauseMenu();
	}
}

void UDicePauseMenuWidget::HandleQuitClicked()
{
	if (ADiceRollPlayerController* Controller = Cast<ADiceRollPlayerController>(GetOwningPlayer()))
	{
		Controller->QuitFromPauseMenu();
	}
}
