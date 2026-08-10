// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RollOnlyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

/** Подготавливает единственную кнопку, не добавляя progression, магазин или другие элементы HUD. */
void URollOnlyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UWidget* RollControl = WidgetTree
		? WidgetTree->FindWidget(TEXT("GenerateBTN"))
		: nullptr;
	RollButton = Cast<UButton>(RollControl);
	if (!IsValid(RollButton) && IsValid(RollControl))
	{
		// GenerateBTN в исходном UI — это W_CustomButton. Ищем его
		// внутренний UButton только для визуальной настройки; его исходный
		// OnClicked-делегат и Blueprint-граф при этом не заменяются.
		if (UUserWidget* CustomButton = Cast<UUserWidget>(RollControl))
		{
			TArray<UWidget*> InnerWidgets;
			if (CustomButton->WidgetTree)
			{
				CustomButton->WidgetTree->GetAllWidgets(InnerWidgets);
			}
			for (UWidget* Widget : InnerWidgets)
			{
				if (UButton* Button = Cast<UButton>(Widget))
				{
					RollButton = Button;
					break;
				}
			}
		}
	}
	if (!IsValid(RollControl))
	{
		BuildFallbackRollButton();
		RollControl = RollButton;
	}
	if (!IsValid(RollControl))
	{
		UE_LOG(LogTemp, Error, TEXT("Roll-only widget failed to create its roll button."));
		return;
	}

	RollControl->SetVisibility(ESlateVisibility::Visible);
	RollControl->SetIsEnabled(true);
	if (UTextBlock* ButtonText = FindFirstTextBlock(RollControl))
	{
		ButtonText->SetText(RollButtonLabel);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateBTN has no TextBlock content to relabel."));
	}
}

/** Строит простой Canvas с одной кнопкой внизу по центру, когда в исходном Widget Blueprint нет Designer-разметки. */
void URollOnlyWidget::BuildFallbackRollButton()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("RollOnlyRoot"));
	WidgetTree->RootWidget = RootCanvas;

	RollButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("NativeRollButton"));
	RollButton->SetBackgroundColor(FLinearColor(0.12f, 0.30f, 0.52f, 0.96f));
	RollButton->OnClicked.AddUniqueDynamic(this, &URollOnlyWidget::HandleRollButtonClicked);

	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("RollButtonText"));
	ButtonText->SetText(RollButtonLabel);
	ButtonText->SetJustification(ETextJustify::Center);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo ButtonFont = ButtonText->GetFont();
	ButtonFont.Size = 24;
	ButtonText->SetFont(ButtonFont);
	RollButton->SetContent(ButtonText);

	UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(RollButton);
	ButtonSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	ButtonSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	ButtonSlot->SetPosition(FVector2D(0.0f, -36.0f));
	ButtonSlot->SetSize(FVector2D(260.0f, 64.0f));
}

/** Находит акторов с уже реализованной RollDice и передаёт им клик, не вмешиваясь в физику или scoring. */
void URollOnlyWidget::HandleRollButtonClicked()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	int32 RolledDiceCount = 0;
	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		UFunction* RollFunction = Actor->FindFunction(TEXT("RollDice"));
		if (IsValid(RollFunction) && RollFunction->ParmsSize == 0)
		{
			Actor->ProcessEvent(RollFunction, nullptr);
			++RolledDiceCount;
		}
	}

	if (RolledDiceCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Roll-only scene found no actors exposing RollDice."));
	}
}

/** Обходит PanelWidget-потомков до первого текста, чтобы поддержать и прямой, и вложенный контент кнопки. */
UTextBlock* URollOnlyWidget::FindFirstTextBlock(UWidget* RootWidget) const
{
	if (!IsValid(RootWidget))
	{
		return nullptr;
	}
	if (UTextBlock* Text = Cast<UTextBlock>(RootWidget))
	{
		return Text;
	}
	if (UUserWidget* NestedWidget = Cast<UUserWidget>(RootWidget))
	{
		return NestedWidget->WidgetTree
			? FindFirstTextBlock(NestedWidget->WidgetTree->RootWidget)
			: nullptr;
	}
	if (UPanelWidget* Panel = Cast<UPanelWidget>(RootWidget))
	{
		for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
		{
			if (UTextBlock* Text = FindFirstTextBlock(Panel->GetChildAt(ChildIndex)))
			{
				return Text;
			}
		}
	}
	return nullptr;
}
