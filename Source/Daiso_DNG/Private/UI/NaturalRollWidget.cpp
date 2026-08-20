// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/NaturalRollWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Dice/CPP_Dice.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "Dice/MouseGatherDiceManager.h"
#include "Dice/NaturalDiceRollManager.h"
#include "EngineUtils.h"

void UNaturalRollWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNaturalRollControls();
	RollManager = FindNaturalRollManager();
	GatherManager = FindMouseGatherManager();
	if (IsValid(RollManager))
	{
		RollManager->OnNaturalRollStarted.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollStarted);
		RollManager->OnNaturalRollFinished.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollFinished);
	}
	if (IsValid(GatherManager))
	{
		GatherManager->OnStatusChanged.AddUniqueDynamic(this, &UNaturalRollWidget::HandleGatherStatusChanged);
	}
}

void UNaturalRollWidget::NativeDestruct()
{
	if (IsValid(RollManager))
	{
		RollManager->OnNaturalRollStarted.RemoveDynamic(this, &UNaturalRollWidget::HandleNaturalRollStarted);
		RollManager->OnNaturalRollFinished.RemoveDynamic(this, &UNaturalRollWidget::HandleNaturalRollFinished);
	}
	if (IsValid(GatherManager))
	{
		GatherManager->OnStatusChanged.RemoveDynamic(this, &UNaturalRollWidget::HandleGatherStatusChanged);
	}
	Super::NativeDestruct();
}

void UNaturalRollWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsValid(RollManager))
	{
		RollManager = FindNaturalRollManager();
		if (IsValid(RollManager))
		{
			RollManager->OnNaturalRollStarted.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollStarted);
			RollManager->OnNaturalRollFinished.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollFinished);
		}
	}
	if (!IsValid(GatherManager))
	{
		GatherManager = FindMouseGatherManager();
		if (IsValid(GatherManager))
		{
			GatherManager->OnStatusChanged.AddUniqueDynamic(this, &UNaturalRollWidget::HandleGatherStatusChanged);
		}
	}

	const bool bWorldRollInProgress = (IsValid(RollManager) && RollManager->IsRolling()) || IsAnyGuidedRollActive();
	const bool bGatherActive = IsValid(GatherManager) && GatherManager->IsInteractionActive();
	const bool bGatherDropping = IsValid(GatherManager) && GatherManager->IsDropping();
	if (IsValid(GuidedRollControl))
	{
		GuidedRollControl->SetIsEnabled(!bWorldRollInProgress && !bGatherActive);
	}
	if (IsValid(NaturalRollButton))
	{
		NaturalRollButton->SetIsEnabled(!bWorldRollInProgress && !bGatherActive);
	}
	if (IsValid(NaturalGatherButton))
	{
		NaturalGatherButton->SetIsEnabled(!bWorldRollInProgress && !bGatherDropping);
	}
	if (IsValid(PredictedGatherButton))
	{
		PredictedGatherButton->SetIsEnabled(!bWorldRollInProgress && !bGatherDropping);
	}
}

void UNaturalRollWidget::BuildNaturalRollControls()
{
	if (!WidgetTree || IsValid(NaturalRollButton))
	{
		return;
	}

	GuidedRollControl = WidgetTree->FindWidget(TEXT("GenerateBTN"));
	UCanvasPanel* Canvas = IsValid(GuidedRollControl)
		? Cast<UCanvasPanel>(GuidedRollControl->GetParent())
		: Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!IsValid(Canvas))
	{
		TArray<UWidget*> Widgets;
		WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (UCanvasPanel* Candidate = Cast<UCanvasPanel>(Widget))
			{
				Canvas = Candidate;
				break;
			}
		}
	}
	if (!IsValid(Canvas))
	{
		UE_LOG(LogTemp, Error, TEXT("Natural roll widget could not find a CanvasPanel for the second button."));
		return;
	}

	float ButtonY = -36.0f;
	if (UCanvasPanelSlot* GuidedSlot = IsValid(GuidedRollControl)
		? Cast<UCanvasPanelSlot>(GuidedRollControl->Slot) : nullptr)
	{
		ButtonY = GuidedSlot->GetPosition().Y;
		GuidedSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		GuidedSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		GuidedSlot->SetPosition(FVector2D(-145.0f, ButtonY));
		GuidedSlot->SetSize(FVector2D(260.0f, 64.0f));
	}

	NaturalRollButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NaturalRollButton"));
	NaturalRollButton->SetBackgroundColor(FLinearColor(0.58f, 0.17f, 0.035f, 0.98f));
	NaturalRollButton->SetToolTipText(NSLOCTEXT("NaturalRollWidget", "NaturalRollTooltip",
		"Результат не выбирается заранее: он считывается с верхней грани после остановки."));
	NaturalRollButton->OnClicked.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollClicked);

	NaturalRollButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("NaturalRollButtonText"));
	NaturalRollButtonText->SetText(NSLOCTEXT("NaturalRollWidget", "NaturalRollButtonLabel", "Бросить с хаосом"));
	NaturalRollButtonText->SetJustification(ETextJustify::Center);
	NaturalRollButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo ButtonFont = NaturalRollButtonText->GetFont();
	ButtonFont.Size = 22;
	NaturalRollButtonText->SetFont(ButtonFont);
	NaturalRollButton->SetContent(NaturalRollButtonText);

	UCanvasPanelSlot* NaturalSlot = Canvas->AddChildToCanvas(NaturalRollButton);
	NaturalSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	NaturalSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	NaturalSlot->SetPosition(FVector2D(145.0f, ButtonY));
	NaturalSlot->SetSize(FVector2D(260.0f, 64.0f));

	const float GatherButtonY = ButtonY - 76.0f;
	NaturalGatherButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("NaturalGatherButton"));
	NaturalGatherButton->SetBackgroundColor(FLinearColor(0.08f, 0.42f, 0.23f, 0.98f));
	NaturalGatherButton->SetToolTipText(NSLOCTEXT("NaturalRollWidget", "NaturalGatherTooltip",
		"Соберите кубики курсором и уроните их. Результат считывается только после остановки."));
	NaturalGatherButton->OnClicked.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalGatherClicked);
	UTextBlock* NaturalGatherText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("NaturalGatherButtonText"));
	NaturalGatherText->SetText(NSLOCTEXT("NaturalRollWidget", "NaturalGatherLabel", "Собрать: честно"));
	NaturalGatherText->SetJustification(ETextJustify::Center);
	NaturalGatherText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo GatherFont = NaturalGatherText->GetFont();
	GatherFont.Size = 20;
	NaturalGatherText->SetFont(GatherFont);
	NaturalGatherButton->SetContent(NaturalGatherText);
	UCanvasPanelSlot* NaturalGatherSlot = Canvas->AddChildToCanvas(NaturalGatherButton);
	NaturalGatherSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	NaturalGatherSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	NaturalGatherSlot->SetPosition(FVector2D(-145.0f, GatherButtonY));
	NaturalGatherSlot->SetSize(FVector2D(260.0f, 64.0f));

	PredictedGatherButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("PredictedGatherButton"));
	PredictedGatherButton->SetBackgroundColor(FLinearColor(0.34f, 0.12f, 0.52f, 0.98f));
	PredictedGatherButton->SetToolTipText(NSLOCTEXT("NaturalRollWidget", "PredictedGatherTooltip",
		"Соберите кубики курсором; при падении грани выбираются заранее и направляются физикой."));
	PredictedGatherButton->OnClicked.AddUniqueDynamic(this, &UNaturalRollWidget::HandlePredictedGatherClicked);
	UTextBlock* PredictedGatherText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PredictedGatherButtonText"));
	PredictedGatherText->SetText(NSLOCTEXT("NaturalRollWidget", "PredictedGatherLabel", "Собрать: прогноз"));
	PredictedGatherText->SetJustification(ETextJustify::Center);
	PredictedGatherText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PredictedGatherText->SetFont(GatherFont);
	PredictedGatherButton->SetContent(PredictedGatherText);
	UCanvasPanelSlot* PredictedGatherSlot = Canvas->AddChildToCanvas(PredictedGatherButton);
	PredictedGatherSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	PredictedGatherSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	PredictedGatherSlot->SetPosition(FVector2D(145.0f, GatherButtonY));
	PredictedGatherSlot->SetSize(FVector2D(260.0f, 64.0f));

	NaturalRollStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("NaturalRollStatusText"));
	NaturalRollStatusText->SetText(NSLOCTEXT("NaturalRollWidget", "NaturalRollReady",
		"Оранжевая кнопка: число определяется только после приземления"));
	NaturalRollStatusText->SetJustification(ETextJustify::Center);
	NaturalRollStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.62f, 1.0f)));
	FSlateFontInfo StatusFont = NaturalRollStatusText->GetFont();
	StatusFont.Size = 17;
	NaturalRollStatusText->SetFont(StatusFont);
	UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(NaturalRollStatusText);
	StatusSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	StatusSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	StatusSlot->SetPosition(FVector2D(0.0f, ButtonY - 148.0f));
	StatusSlot->SetSize(FVector2D(680.0f, 34.0f));
}

void UNaturalRollWidget::HandleNaturalGatherClicked()
{
	if (!IsValid(GatherManager))
	{
		GatherManager = FindMouseGatherManager();
	}
	if (IsValid(GatherManager))
	{
		GatherManager->OnStatusChanged.AddUniqueDynamic(this, &UNaturalRollWidget::HandleGatherStatusChanged);
		GatherManager->BeginGather(EMouseGatherDropMode::Natural);
	}
}

void UNaturalRollWidget::HandlePredictedGatherClicked()
{
	if (!IsValid(GatherManager))
	{
		GatherManager = FindMouseGatherManager();
	}
	if (IsValid(GatherManager))
	{
		GatherManager->OnStatusChanged.AddUniqueDynamic(this, &UNaturalRollWidget::HandleGatherStatusChanged);
		GatherManager->BeginGather(EMouseGatherDropMode::Predicted);
	}
}

void UNaturalRollWidget::HandleGatherStatusChanged(const FText Status)
{
	if (IsValid(NaturalRollStatusText))
	{
		NaturalRollStatusText->SetText(Status);
	}
}

void UNaturalRollWidget::HandleNaturalRollClicked()
{
	if (!IsValid(RollManager))
	{
		RollManager = FindNaturalRollManager();
	}
	if (!IsValid(RollManager) && GetWorld())
	{
		RollManager = GetWorld()->SpawnActor<ANaturalDiceRollManager>();
	}
	if (!IsValid(RollManager))
	{
		if (IsValid(NaturalRollStatusText))
		{
			NaturalRollStatusText->SetText(NSLOCTEXT("NaturalRollWidget", "NaturalRollUnavailable",
				"Не удалось запустить физический бросок"));
		}
		return;
	}

	RollManager->OnNaturalRollStarted.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollStarted);
	RollManager->OnNaturalRollFinished.AddUniqueDynamic(this, &UNaturalRollWidget::HandleNaturalRollFinished);
	RollManager->RollAllDice();
}

void UNaturalRollWidget::HandleNaturalRollStarted()
{
	if (IsValid(NaturalRollStatusText))
	{
		NaturalRollStatusText->SetText(NSLOCTEXT("NaturalRollWidget", "NaturalRollInProgress",
			"Кубики сталкиваются — результат пока неизвестен…"));
	}
}

void UNaturalRollWidget::HandleNaturalRollFinished(const TArray<int32>& Results)
{
	FString ResultString;
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		if (Index > 0)
		{
			ResultString += TEXT("  ·  ");
		}
		ResultString += FString::FromInt(Results[Index]);
	}
	if (IsValid(NaturalRollStatusText))
	{
		NaturalRollStatusText->SetText(FText::Format(
			NSLOCTEXT("NaturalRollWidget", "NaturalRollResult", "Выпало после остановки: {0}"),
			FText::FromString(ResultString)));
	}
}

ANaturalDiceRollManager* UNaturalRollWidget::FindNaturalRollManager() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ANaturalDiceRollManager> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

AMouseGatherDiceManager* UNaturalRollWidget::FindMouseGatherManager() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AMouseGatherDiceManager> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

bool UNaturalRollWidget::IsAnyGuidedRollActive() const
{
	if (!GetWorld())
	{
		return false;
	}
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		if (const UDicePhysicsRollComponent* Roll = It->FindComponentByClass<UDicePhysicsRollComponent>();
			IsValid(Roll) && Roll->IsRolling())
		{
			return true;
		}
	}
	return false;
}
