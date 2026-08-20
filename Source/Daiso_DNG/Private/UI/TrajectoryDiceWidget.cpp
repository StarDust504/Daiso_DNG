// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TrajectoryDiceWidget.h"

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
#include "Dice/CPP_Dice.h"
#include "Dice/MouseGatherDiceManager.h"
#include "Dice/SpectacleDiceRollManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameManagerSubsystem.h"

namespace TrajectoryDiceStyle
{
	static UTextBlock* MakeText(UWidgetTree& Tree, const FName Name, const int32 FontSize,
		const FLinearColor Color, const ETextJustify::Type Justification = ETextJustify::Center)
	{
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		return Text;
	}
}

void UTrajectoryDiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildInterface();
}

void UTrajectoryDiceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildInterface();
	ResolveAndBindManagers();
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->OnDiceSelectionChanged.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleScoreChanged);
		Manager->OnLevelProgressChanged.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleProgressChanged);
		bPreviousStoreOpen = Manager->IsStoreOpen();
	}
	RefreshProgress();
	RefreshResultStrip();
}

void UTrajectoryDiceWidget::NativeDestruct()
{
	if (IsValid(GatherManager))
	{
		GatherManager->OnStatusChanged.RemoveDynamic(this, &UTrajectoryDiceWidget::HandleGatherStatusChanged);
		GatherManager->OnFinished.RemoveDynamic(this, &UTrajectoryDiceWidget::HandleGatherRollFinished);
	}
	if (IsValid(SpectacleManager))
	{
		SpectacleManager->OnStatusChanged.RemoveDynamic(this, &UTrajectoryDiceWidget::HandleSpectacleStatusChanged);
		SpectacleManager->OnFinished.RemoveDynamic(this, &UTrajectoryDiceWidget::HandleSpectacleRollFinished);
	}
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->OnDiceSelectionChanged.RemoveDynamic(this, &UTrajectoryDiceWidget::HandleScoreChanged);
		Manager->OnLevelProgressChanged.RemoveDynamic(this, &UTrajectoryDiceWidget::HandleProgressChanged);
	}
	Super::NativeDestruct();
}

void UTrajectoryDiceWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ResolveAndBindManagers();
	const UGameManagerSubsystem* Manager = ResolveGameManager();
	const bool bRunBlocked = !IsValid(Manager) || Manager->IsStoreOpen() || Manager->IsGameOver();
	const bool bRollActive = IsAnyRollActive();
	APlayerController* PlayerController = GetOwningPlayer();
	const bool bLeftMouseDown = IsValid(PlayerController)
		&& PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (!bRunBlocked && !bRollActive && bLeftMouseDown && !bPreviousSelectionClickDown
		&& !IsPointerOverControls())
	{
		TryToggleDieUnderCursor();
	}
	bPreviousSelectionClickDown = bLeftMouseDown;
	if (IsValid(ManualGatherButton))
	{
		ManualGatherButton->SetIsEnabled(!bRunBlocked && !bRollActive);
	}
	if (IsValid(AutomaticGatherButton))
	{
		AutomaticGatherButton->SetIsEnabled(!bRunBlocked && !bRollActive);
	}
	if (IsValid(FinishRoundButton) && IsValid(Manager))
	{
		FinishRoundButton->SetIsEnabled(!bRollActive && Manager->GetLevelProgress().bCanFinishRound);
	}
}

/** Создаёт независимый HUD, не наследуя переполненную панель экспериментальных режимов. */
void UTrajectoryDiceWidget::BuildInterface()
{
	if (!WidgetTree || IsValid(StatusText))
	{
		return;
	}
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("TrajectoryDiceRoot"));
	WidgetTree->RootWidget = Canvas;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UBorder* ProgressPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TrajectoryProgressPanel"));
	ProgressPanel->SetBrushColor(FLinearColor(0.018f, 0.029f, 0.052f, 0.91f));
	ProgressPanel->SetPadding(FMargin(16.0f, 12.0f));
	UCanvasPanelSlot* ProgressSlot = Canvas->AddChildToCanvas(ProgressPanel);
	ProgressSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	ProgressSlot->SetPosition(FVector2D(30.0f, 26.0f));
	ProgressSlot->SetSize(FVector2D(350.0f, 224.0f));

	UVerticalBox* ProgressBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("TrajectoryProgressBox"));
	ProgressPanel->SetContent(ProgressBox);
	RoundText = TrajectoryDiceStyle::MakeText(*WidgetTree, TEXT("TrajectoryRoundText"), 19,
		FLinearColor(0.66f, 0.84f, 1.0f, 1.0f), ETextJustify::Left);
	ScoreText = TrajectoryDiceStyle::MakeText(*WidgetTree, TEXT("TrajectoryScoreText"), 28,
		FLinearColor::White, ETextJustify::Left);
	MoneyText = TrajectoryDiceStyle::MakeText(*WidgetTree, TEXT("TrajectoryMoneyText"), 20,
		FLinearColor(1.0f, 0.82f, 0.25f, 1.0f), ETextJustify::Left);
	SelectionHintText = TrajectoryDiceStyle::MakeText(*WidgetTree, TEXT("TrajectorySelectionHint"), 15,
		FLinearColor(0.78f, 0.84f, 0.92f, 1.0f), ETextJustify::Left);
	ProgressBox->AddChildToVerticalBox(RoundText);
	ProgressBox->AddChildToVerticalBox(ScoreText);
	ProgressBox->AddChildToVerticalBox(MoneyText);
	ProgressBox->AddChildToVerticalBox(SelectionHintText);

	StatusText = TrajectoryDiceStyle::MakeText(*WidgetTree, TEXT("TrajectoryStatusText"), 19,
		FLinearColor(0.91f, 0.94f, 1.0f, 1.0f));
	StatusText->SetText(NSLOCTEXT("TrajectoryDiceWidget", "Ready",
		"Выберите ручной или автоматический сбор кубиков"));
	UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(StatusText);
	StatusSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	StatusSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	StatusSlot->SetPosition(FVector2D(0.0f, 32.0f));
	StatusSlot->SetSize(FVector2D(720.0f, 72.0f));

	ResultsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TrajectoryResultsPanel"));
	ResultsPanel->SetBrushColor(FLinearColor(0.025f, 0.039f, 0.068f, 0.92f));
	ResultsPanel->SetPadding(FMargin(12.0f, 8.0f));
	UCanvasPanelSlot* ResultsPanelSlot = Canvas->AddChildToCanvas(ResultsPanel);
	ResultsPanelSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	ResultsPanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	ResultsPanelSlot->SetPosition(FVector2D(0.0f, -126.0f));
	ResultsPanelSlot->SetSize(FVector2D(500.0f, 66.0f));
	UHorizontalBox* ResultRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("TrajectoryResultRow"));
	ResultsPanel->SetContent(ResultRow);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
			FName(*FString::Printf(TEXT("TrajectoryResultButton%d"), Index)));
		UTextBlock* Text = TrajectoryDiceStyle::MakeText(*WidgetTree,
			FName(*FString::Printf(TEXT("TrajectoryResultText%d"), Index)), 22, FLinearColor::White);
		Button->SetContent(Text);
		Button->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBoxSlot* ButtonSlot = ResultRow->AddChildToHorizontalBox(Button);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ButtonSlot->SetPadding(FMargin(5.0f, 0.0f));
		ResultButtons.Add(Button);
		ResultButtonTexts.Add(Text);
	}
	ResultButtons[0]->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleResult0Clicked);
	ResultButtons[1]->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleResult1Clicked);
	ResultButtons[2]->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleResult2Clicked);
	ResultButtons[3]->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleResult3Clicked);
	ResultButtons[4]->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleResult4Clicked);
	ResultButtons[5]->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleResult5Clicked);

	ManualGatherButton = AddActionButton(Canvas, TEXT("ManualTrajectoryGatherButton"),
		NSLOCTEXT("TrajectoryDiceWidget", "ManualLabel", "1  Ручной сбор + бросок"),
		NSLOCTEXT("TrajectoryDiceWidget", "ManualTooltip",
			"Проведите по нужным кубикам, затем удерживайте ЛКМ, сделайте жест и отпустите."),
		FLinearColor(0.06f, 0.39f, 0.27f, 0.98f), -315.0f, 290.0f);
	AutomaticGatherButton = AddActionButton(Canvas, TEXT("AutomaticTrajectoryGatherButton"),
		NSLOCTEXT("TrajectoryDiceWidget", "AutomaticLabel", "2  Автосбор + бросок"),
		NSLOCTEXT("TrajectoryDiceWidget", "AutomaticTooltip",
			"Все невыбранные кубики автоматически собираются у курсора; удерживайте ЛКМ и отпустите после жеста."),
		FLinearColor(0.13f, 0.30f, 0.59f, 0.98f), 0.0f, 290.0f);
	FinishRoundButton = AddActionButton(Canvas, TEXT("TrajectoryFinishRoundButton"),
		NSLOCTEXT("TrajectoryDiceWidget", "FinishLabel", "Завершить раунд"),
		NSLOCTEXT("TrajectoryDiceWidget", "FinishTooltip",
			"Зафиксировать выбранные результативные кубики и открыть магазин."),
		FLinearColor(0.48f, 0.24f, 0.055f, 0.98f), 315.0f, 250.0f);
	FinishRoundButtonText = Cast<UTextBlock>(FinishRoundButton->GetContent());
	ManualGatherButton->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleManualGatherClicked);
	AutomaticGatherButton->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleAutomaticGatherClicked);
	FinishRoundButton->OnClicked.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleFinishRoundClicked);
}

UButton* UTrajectoryDiceWidget::AddActionButton(UCanvasPanel* Canvas, const FName Name,
	const FText& Label, const FText& Tooltip, const FLinearColor& Color, const float X, const float Width)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(Color);
	Button->SetToolTipText(Tooltip);
	UTextBlock* LabelText = TrajectoryDiceStyle::MakeText(*WidgetTree,
		FName(*(Name.ToString() + TEXT("Text"))), 19, FLinearColor::White);
	LabelText->SetText(Label);
	Button->SetContent(LabelText);
	UCanvasPanelSlot* ButtonCanvasSlot = Canvas->AddChildToCanvas(Button);
	ButtonCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	ButtonCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	ButtonCanvasSlot->SetPosition(FVector2D(X, -42.0f));
	ButtonCanvasSlot->SetSize(FVector2D(Width, 64.0f));
	return Button;
}

void UTrajectoryDiceWidget::ResolveAndBindManagers()
{
	if (!IsValid(GatherManager))
	{
		GatherManager = FindGatherManager();
		if (IsValid(GatherManager))
		{
			GatherManager->OnStatusChanged.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleGatherStatusChanged);
			GatherManager->OnFinished.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleGatherRollFinished);
		}
	}
	if (!IsValid(SpectacleManager))
	{
		SpectacleManager = FindSpectacleManager();
		if (IsValid(SpectacleManager))
		{
			SpectacleManager->OnStatusChanged.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleSpectacleStatusChanged);
			SpectacleManager->OnFinished.AddUniqueDynamic(this, &UTrajectoryDiceWidget::HandleSpectacleRollFinished);
		}
	}
}

AMouseGatherDiceManager* UTrajectoryDiceWidget::FindGatherManager() const
{
	if (GetWorld())
	{
		for (TActorIterator<AMouseGatherDiceManager> It(GetWorld()); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

ASpectacleDiceRollManager* UTrajectoryDiceWidget::FindSpectacleManager() const
{
	if (GetWorld())
	{
		for (TActorIterator<ASpectacleDiceRollManager> It(GetWorld()); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

UGameManagerSubsystem* UTrajectoryDiceWidget::ResolveGameManager() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UGameManagerSubsystem>() : nullptr;
}

bool UTrajectoryDiceWidget::IsAnyRollActive() const
{
	return (IsValid(GatherManager) && GatherManager->IsInteractionActive())
		|| (IsValid(SpectacleManager) && SpectacleManager->IsActive());
}

void UTrajectoryDiceWidget::PrepareForReroll()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->NotifyDiceRollStarted();
	}
	for (int32 Index = 0; Index < LastRollResults.Num(); ++Index)
	{
		if (!SelectedResults.IsValidIndex(Index) || !SelectedResults[Index])
		{
			LastRollResults[Index] = 0;
		}
	}
	SubmitSelectedDice();
}

/** Обновляет только переброшенные кости; уже удержанные акторы и их значения остаются неизменными. */
void UTrajectoryDiceWidget::AutoSelectScoringDice(
	const TArray<int32>& Results, const TArray<ACPP_Dice*>& DiceActors)
{
	const TArray<bool> HeldBeforeRoll = SelectedResults;
	TSet<ACPP_Dice*> FinishedDice;
	const int32 FinishedCount = FMath::Min(Results.Num(), DiceActors.Num());
	for (int32 ResultIndex = 0; ResultIndex < FinishedCount; ++ResultIndex)
	{
		ACPP_Dice* Dice = DiceActors[ResultIndex];
		if (!IsValid(Dice))
		{
			continue;
		}
		int32 Index = LastRollDice.IndexOfByPredicate([Dice](const TObjectPtr<ACPP_Dice>& Candidate)
		{
			return Candidate.Get() == Dice;
		});
		if (Index == INDEX_NONE)
		{
			Index = LastRollDice.Add(Dice);
			LastRollResults.Add(0);
			SelectedResults.Add(false);
		}
		LastRollResults[Index] = Results[ResultIndex] >= 1 && Results[ResultIndex] <= 6
			? Results[ResultIndex]
			: 0;
		SelectedResults[Index] = LastRollResults[Index] > 0;
		FinishedDice.Add(Dice);
	}

	UGameManagerSubsystem* Manager = ResolveGameManager();
	if (!IsValid(Manager))
	{
		return;
	}
	TArray<int32> CandidateSelection;
	for (int32 Index = 0; Index < LastRollResults.Num(); ++Index)
	{
		const bool bWasHeld = HeldBeforeRoll.IsValidIndex(Index) && HeldBeforeRoll[Index];
		if ((bWasHeld || SelectedResults.IsValidIndex(Index) && SelectedResults[Index])
			&& LastRollResults[Index] >= 1 && LastRollResults[Index] <= 6)
		{
			SelectedResults[Index] = true;
			CandidateSelection.Add(LastRollResults[Index]);
		}
	}
	if (!Manager->SetDiceRollSelection(CandidateSelection))
	{
		return;
	}

	TArray<int32> RemainingUnscored = Manager->GetSelectedDiceScore().UnscoredDiceValues;
	for (int32 Index = 0; Index < LastRollResults.Num(); ++Index)
	{
		ACPP_Dice* Dice = LastRollDice.IsValidIndex(Index) ? LastRollDice[Index].Get() : nullptr;
		const bool bWasHeld = HeldBeforeRoll.IsValidIndex(Index) && HeldBeforeRoll[Index];
		if (bWasHeld || !FinishedDice.Contains(Dice))
		{
			continue;
		}
		const int32 Match = RemainingUnscored.Find(LastRollResults[Index]);
		if (Match != INDEX_NONE)
		{
			SelectedResults[Index] = false;
			RemainingUnscored.RemoveAt(Match);
		}
	}
	SubmitSelectedDice();
}

void UTrajectoryDiceWidget::SubmitSelectedDice()
{
	TArray<int32> Selection;
	for (int32 Index = 0; Index < LastRollResults.Num(); ++Index)
	{
		if (SelectedResults.IsValidIndex(Index) && SelectedResults[Index]
			&& LastRollResults[Index] >= 1 && LastRollResults[Index] <= 6)
		{
			Selection.Add(LastRollResults[Index]);
		}
	}
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		if (Manager->SetDiceRollSelection(Selection))
		{
			SyncPhysicalSelection();
		}
	}
	RefreshResultStrip();
	RefreshProgress();
}

void UTrajectoryDiceWidget::SyncPhysicalSelection()
{
	UGameManagerSubsystem* Manager = ResolveGameManager();
	if (!IsValid(Manager))
	{
		return;
	}
	for (int32 Index = 0; Index < LastRollDice.Num(); ++Index)
	{
		ACPP_Dice* Dice = LastRollDice[Index].Get();
		if (!IsValid(Dice))
		{
			continue;
		}
		const bool bSelected = SelectedResults.IsValidIndex(Index) && SelectedResults[Index]
			&& LastRollResults.IsValidIndex(Index)
			&& LastRollResults[Index] >= 1 && LastRollResults[Index] <= 6;
		Dice->SetIsActive(bSelected);
		if (bSelected && !Manager->CheckIsDiceRegistered(Dice))
		{
			Manager->RegisterDice(Dice);
		}
		else if (!bSelected && Manager->CheckIsDiceRegistered(Dice))
		{
			Manager->UnregisterDice(Dice);
		}
	}
}

bool UTrajectoryDiceWidget::ToggleResult(const int32 Index)
{
	UGameManagerSubsystem* Manager = ResolveGameManager();
	if (!SelectedResults.IsValidIndex(Index) || !LastRollResults.IsValidIndex(Index)
		|| LastRollResults[Index] < 1 || LastRollResults[Index] > 6 || !IsValid(Manager)
		|| Manager->IsStoreOpen() || Manager->IsGameOver() || IsAnyRollActive())
	{
		return false;
	}
	SelectedResults[Index] = !SelectedResults[Index];
	SubmitSelectedDice();
	return true;
}

bool UTrajectoryDiceWidget::TogglePhysicalDiceSelection(ACPP_Dice* Dice)
{
	if (!IsValid(Dice))
	{
		return false;
	}
	const int32 Index = LastRollDice.IndexOfByPredicate([Dice](const TObjectPtr<ACPP_Dice>& Candidate)
	{
		return Candidate.Get() == Dice;
	});
	return ToggleResult(Index);
}

bool UTrajectoryDiceWidget::TryToggleDieUnderCursor()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return false;
	}

	FHitResult Hit;
	if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		ACPP_Dice* HitDice = Cast<ACPP_Dice>(Hit.GetActor());
		if (IsValid(HitDice) && TogglePhysicalDiceSelection(HitDice))
		{
			const bool bSelected = HitDice->GetIsActive();
			StatusText->SetText(bSelected
				? NSLOCTEXT("TrajectoryDiceWidget", "PhysicalDieHeld",
					"Кубик выбран и останется на столе при следующем броске")
				: NSLOCTEXT("TrajectoryDiceWidget", "PhysicalDieReleased",
					"Выбор снят: кубик снова будет участвовать в броске"));
			return true;
		}
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	ACPP_Dice* ClosestDice = nullptr;
	float ClosestDistanceSquared = FMath::Square(42.0f);
	for (int32 Index = 0; Index < LastRollDice.Num(); ++Index)
	{
		ACPP_Dice* Dice = LastRollDice[Index].Get();
		if (!IsValid(Dice) || !LastRollResults.IsValidIndex(Index) || LastRollResults[Index] <= 0)
		{
			continue;
		}
		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(
			Dice->SMC_Dice->GetComponentLocation(), ScreenPosition, true))
		{
			continue;
		}
		const float DistanceSquared = FVector2D::DistSquared(ScreenPosition, FVector2D(MouseX, MouseY));
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestDice = Dice;
		}
	}
	if (IsValid(ClosestDice) && TogglePhysicalDiceSelection(ClosestDice))
	{
		StatusText->SetText(ClosestDice->GetIsActive()
			? NSLOCTEXT("TrajectoryDiceWidget", "PhysicalDieHeldFallback",
				"Кубик выбран и останется на столе при следующем броске")
			: NSLOCTEXT("TrajectoryDiceWidget", "PhysicalDieReleasedFallback",
				"Выбор снят: кубик снова будет участвовать в броске"));
		return true;
	}
	return false;
}

bool UTrajectoryDiceWidget::IsPointerOverControls() const
{
	if (IsValid(ManualGatherButton) && ManualGatherButton->IsHovered()
		|| IsValid(AutomaticGatherButton) && AutomaticGatherButton->IsHovered()
		|| IsValid(FinishRoundButton) && FinishRoundButton->IsHovered())
	{
		return true;
	}
	return ResultButtons.ContainsByPredicate([](const TObjectPtr<UButton>& Button)
	{
		return IsValid(Button) && Button->IsHovered();
	});
}

void UTrajectoryDiceWidget::RefreshProgress()
{
	UGameManagerSubsystem* Manager = ResolveGameManager();
	if (!IsValid(Manager) || !IsValid(RoundText))
	{
		return;
	}
	const FLevelProgressState Progress = Manager->GetLevelProgress();
	RoundText->SetText(FText::Format(NSLOCTEXT("TrajectoryDiceWidget", "RoundGoal",
		"Раунд {0}  •  цель {1}"), FText::AsNumber(Progress.LevelNumber), FText::AsNumber(Progress.TargetScore)));
	ScoreText->SetText(FText::Format(NSLOCTEXT("TrajectoryDiceWidget", "Score",
		"Счёт: {0} / {1}"), FText::AsNumber(Progress.CurrentScore), FText::AsNumber(Progress.TargetScore)));
	MoneyText->SetText(FText::Format(NSLOCTEXT("TrajectoryDiceWidget", "Money", "Монеты: {0}"),
		FText::AsNumber(Progress.Money)));
	const bool bHasRollResult = LastRollResults.ContainsByPredicate([](const int32 Value)
	{
		return Value >= 1 && Value <= 6;
	});
	SelectionHintText->SetText(!bHasRollResult
		? NSLOCTEXT("TrajectoryDiceWidget", "NoResults", "После броска выберите кубики в панели или кликом на столе")
		: Manager->IsCurrentDiceSelectionValid()
			? NSLOCTEXT("TrajectoryDiceWidget", "ValidSelection", "Выбранные кубики останутся при перебросе")
			: NSLOCTEXT("TrajectoryDiceWidget", "InvalidSelection", "Оставьте только результативные кубики"));
	if (IsValid(FinishRoundButton))
	{
		FinishRoundButton->SetIsEnabled(Progress.bCanFinishRound && !IsAnyRollActive());
	}
	if (bPreviousStoreOpen && !Progress.bInStore)
	{
		LastRollResults.Reset();
		LastRollDice.Reset();
		SelectedResults.Reset();
		RefreshResultStrip();
		StatusText->SetText(NSLOCTEXT("TrajectoryDiceWidget", "NextRoundReady",
			"Новый раунд: выберите способ сбора и броска"));
	}
	bPreviousStoreOpen = Progress.bInStore;
}

void UTrajectoryDiceWidget::RefreshResultStrip()
{
	const bool bHasAnyResult = LastRollResults.ContainsByPredicate([](const int32 Value)
	{
		return Value >= 1 && Value <= 6;
	});
	if (IsValid(ResultsPanel))
	{
		ResultsPanel->SetVisibility(!bHasAnyResult
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
	for (int32 Index = 0; Index < ResultButtons.Num(); ++Index)
	{
		const bool bHasResult = LastRollResults.IsValidIndex(Index)
			&& LastRollResults[Index] >= 1 && LastRollResults[Index] <= 6;
		ResultButtons[Index]->SetVisibility(bHasResult ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bHasResult)
		{
			continue;
		}
		const bool bSelected = SelectedResults.IsValidIndex(Index) && SelectedResults[Index];
		ResultButtons[Index]->SetBackgroundColor(bSelected
			? FLinearColor(0.10f, 0.50f, 0.29f, 1.0f)
			: FLinearColor(0.18f, 0.20f, 0.26f, 0.92f));
		ResultButtonTexts[Index]->SetText(FText::FromString(bSelected
			? FString::Printf(TEXT("[%d]"), LastRollResults[Index])
			: FString::FromInt(LastRollResults[Index])));
	}
}

void UTrajectoryDiceWidget::HandleManualGatherClicked()
{
	PrepareForReroll();
	ResolveAndBindManagers();
	if (IsValid(GatherManager))
	{
		GatherManager->BeginTrajectoryGather(EMouseGatherDropMode::Natural);
	}
}

void UTrajectoryDiceWidget::HandleAutomaticGatherClicked()
{
	PrepareForReroll();
	ResolveAndBindManagers();
	if (IsValid(SpectacleManager))
	{
		SpectacleManager->StartHandful();
	}
}

void UTrajectoryDiceWidget::HandleFinishRoundClicked()
{
	if (UGameManagerSubsystem* Manager = ResolveGameManager())
	{
		Manager->FinishRound();
	}
}

void UTrajectoryDiceWidget::HandleGatherStatusChanged(const FText Status)
{
	if (IsValid(StatusText))
	{
		StatusText->SetText(Status);
	}
}

void UTrajectoryDiceWidget::HandleSpectacleStatusChanged(const FText Status)
{
	if (IsValid(StatusText))
	{
		StatusText->SetText(Status);
	}
}

void UTrajectoryDiceWidget::ApplyRollResults(
	const TArray<int32>& Results, const TArray<ACPP_Dice*>& DiceActors)
{
	AutoSelectScoringDice(Results, DiceActors);
	if (UGameManagerSubsystem* MutableManager = ResolveGameManager())
	{
		MutableManager->NotifyDiceRollResolved(Results);
	}
	TArray<FString> Values;
	for (const int32 Value : Results)
	{
		Values.Add(FString::FromInt(Value));
	}
	const UGameManagerSubsystem* Manager = ResolveGameManager();
	const int32 Score = IsValid(Manager) ? Manager->GetSelectedDiceScore().TotalScore : 0;
	StatusText->SetText(Score > 0
		? FText::Format(NSLOCTEXT("TrajectoryDiceWidget", "RollScored",
			"Выпало: {0}  •  автоматически выбрано очков: {1}"),
			FText::FromString(FString::Join(Values, TEXT(" · "))), FText::AsNumber(Score))
		: FText::Format(NSLOCTEXT("TrajectoryDiceWidget", "Farkle",
			"Выпало: {0}  •  нет результативных комбинаций, бросьте снова"),
			FText::FromString(FString::Join(Values, TEXT(" · ")))));
}

void UTrajectoryDiceWidget::HandleGatherRollFinished(const TArray<int32>& Results)
{
	ApplyRollResults(Results, IsValid(GatherManager)
		? GatherManager->GetLastFinishedDice()
		: TArray<ACPP_Dice*>());
}

void UTrajectoryDiceWidget::HandleSpectacleRollFinished(const TArray<int32>& Results)
{
	ApplyRollResults(Results, IsValid(SpectacleManager)
		? SpectacleManager->GetLastFinishedDice()
		: TArray<ACPP_Dice*>());
}

void UTrajectoryDiceWidget::HandleScoreChanged(FDiceRollScoreResult) { RefreshProgress(); }
void UTrajectoryDiceWidget::HandleProgressChanged(FLevelProgressState) { RefreshProgress(); }
void UTrajectoryDiceWidget::HandleResult0Clicked() { ToggleResult(0); }
void UTrajectoryDiceWidget::HandleResult1Clicked() { ToggleResult(1); }
void UTrajectoryDiceWidget::HandleResult2Clicked() { ToggleResult(2); }
void UTrajectoryDiceWidget::HandleResult3Clicked() { ToggleResult(3); }
void UTrajectoryDiceWidget::HandleResult4Clicked() { ToggleResult(4); }
void UTrajectoryDiceWidget::HandleResult5Clicked() { ToggleResult(5); }
