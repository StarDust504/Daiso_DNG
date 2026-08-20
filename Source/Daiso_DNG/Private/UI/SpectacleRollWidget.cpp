// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SpectacleRollWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Dice/CPP_Dice.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "Dice/MouseGatherDiceManager.h"
#include "Dice/NaturalDiceRollManager.h"
#include "Dice/SpectacleDiceRollManager.h"
#include "EngineUtils.h"

void USpectacleRollWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A pure native UUserWidget has no designer tree. Build it before RebuildWidget asks the tree
	// for its Slate root, otherwise a late NativeConstruct-only tree would render as an empty spacer.
	BuildInterface();
}

void USpectacleRollWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Manager = FindManager();
	BindManager();
}

void USpectacleRollWidget::NativeDestruct()
{
	if (IsValid(Manager))
	{
		Manager->OnStatusChanged.RemoveDynamic(this, &USpectacleRollWidget::HandleStatusChanged);
	}
	Super::NativeDestruct();
}

void USpectacleRollWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsValid(Manager))
	{
		Manager = FindManager();
		BindManager();
	}
	const bool bSpecialActive = IsValid(Manager) && Manager->IsActive();
	const bool bCanStart = !bSpecialActive && !IsOtherDiceInteractionActive();
	for (UButton* Button : ModeButtons)
	{
		if (IsValid(Button))
		{
			Button->SetIsEnabled(bCanStart);
		}
	}
	if (IsValid(InteractionBlocker))
	{
		InteractionBlocker->SetVisibility(bSpecialActive
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
}

void USpectacleRollWidget::BuildInterface()
{
	if (!WidgetTree || IsValid(StatusText))
	{
		return;
	}
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!IsValid(Canvas))
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpectacleRoot"));
		WidgetTree->RootWidget = Canvas;
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	InteractionBlocker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SpectacleInteractionBlocker"));
	InteractionBlocker->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.001f));
	InteractionBlocker->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* BlockerSlot = Canvas->AddChildToCanvas(InteractionBlocker);
	BlockerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BlockerSlot->SetOffsets(FMargin(0.0f));
	BlockerSlot->SetZOrder(0);

	UTextBlock* BackboardHeader = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BackboardHeader"));
	BackboardHeader->SetText(NSLOCTEXT("SpectacleRollWidget", "BackboardHeader", "ИЗ-ЗА ДОСКИ"));
	BackboardHeader->SetJustification(ETextJustify::Center);
	BackboardHeader->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.83f, 0.62f, 1.0f)));
	FSlateFontInfo BackboardHeaderFont = BackboardHeader->GetFont();
	BackboardHeaderFont.Size = 19;
	BackboardHeader->SetFont(BackboardHeaderFont);
	UCanvasPanelSlot* BackboardHeaderSlot = Canvas->AddChildToCanvas(BackboardHeader);
	BackboardHeaderSlot->SetAnchors(FAnchors(0.0f, 0.5f));
	BackboardHeaderSlot->SetAlignment(FVector2D(0.0f, 0.5f));
	BackboardHeaderSlot->SetPosition(FVector2D(24.0f, -128.0f));
	BackboardHeaderSlot->SetSize(FVector2D(292.0f, 34.0f));
	BackboardHeaderSlot->SetZOrder(2);

	UButton* BackboardNatural = AddModeButton(Canvas, TEXT("BackboardNaturalButton"),
		NSLOCTEXT("SpectacleRollWidget", "BackboardNaturalLabel", "Из-за доски: хаос"),
		NSLOCTEXT("SpectacleRollWidget", "BackboardNaturalTooltip",
			"Автоматический физический бросок через дальний край; результат заранее неизвестен."),
		FLinearColor(0.68f, 0.24f, 0.035f, 0.98f), -76.0f, true);
	BackboardNatural->OnClicked.AddUniqueDynamic(
		this, &USpectacleRollWidget::HandleBackboardNaturalClicked);

	UButton* BackboardPredicted = AddModeButton(Canvas, TEXT("BackboardPredictedButton"),
		NSLOCTEXT("SpectacleRollWidget", "BackboardPredictedLabel", "Из-за доски: прогноз"),
		NSLOCTEXT("SpectacleRollWidget", "BackboardPredictedTooltip",
			"Автоматический вход из-за доски с физическим наведением выбранных граней."),
		FLinearColor(0.42f, 0.14f, 0.62f, 0.98f), -14.0f, true);
	BackboardPredicted->OnClicked.AddUniqueDynamic(
		this, &USpectacleRollWidget::HandleBackboardPredictedClicked);

	UButton* BackboardDirected = AddModeButton(Canvas, TEXT("BackboardDirectedButton"),
		NSLOCTEXT("SpectacleRollWidget", "BackboardDirectedLabel", "Из-за доски: по ЛКМ"),
		NSLOCTEXT("SpectacleRollWidget", "BackboardDirectedTooltip",
			"Подготовьте кубики, затем щёлкните ЛКМ по желаемой точке доски."),
		FLinearColor(0.10f, 0.45f, 0.30f, 0.98f), 48.0f, true);
	BackboardDirected->OnClicked.AddUniqueDynamic(
		this, &USpectacleRollWidget::HandleBackboardDirectedClicked);

	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpectacleHeader"));
	Header->SetText(NSLOCTEXT("SpectacleRollWidget", "Header", "ЭФФЕКТНЫЕ БРОСКИ"));
	Header->SetJustification(ETextJustify::Center);
	Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.91f, 1.0f, 1.0f)));
	FSlateFontInfo HeaderFont = Header->GetFont();
	HeaderFont.Size = 19;
	Header->SetFont(HeaderFont);
	UCanvasPanelSlot* HeaderSlot = Canvas->AddChildToCanvas(Header);
	HeaderSlot->SetAnchors(FAnchors(1.0f, 0.5f));
	HeaderSlot->SetAlignment(FVector2D(1.0f, 0.5f));
	HeaderSlot->SetPosition(FVector2D(-24.0f, -190.0f));
	HeaderSlot->SetSize(FVector2D(292.0f, 34.0f));
	HeaderSlot->SetZOrder(2);

	UButton* VortexNatural = AddModeButton(Canvas, TEXT("VortexNaturalButton"),
		NSLOCTEXT("SpectacleRollWidget", "VortexNaturalLabel", "Вихрь: честно"),
		NSLOCTEXT("SpectacleRollWidget", "VortexNaturalTooltip",
			"Кубики физически вращаются вокруг курсора; результат читается только после остановки."),
		FLinearColor(0.03f, 0.42f, 0.56f, 0.98f), -138.0f);
	VortexNatural->OnClicked.AddUniqueDynamic(this, &USpectacleRollWidget::HandleVortexNaturalClicked);

	UButton* VortexPredicted = AddModeButton(Canvas, TEXT("VortexPredictedButton"),
		NSLOCTEXT("SpectacleRollWidget", "VortexPredictedLabel", "Вихрь: прогноз"),
		NSLOCTEXT("SpectacleRollWidget", "VortexPredictedTooltip",
			"Тот же физический вихрь, но при выпуске выбираются целевые грани."),
		FLinearColor(0.31f, 0.13f, 0.62f, 0.98f), -76.0f);
	VortexPredicted->OnClicked.AddUniqueDynamic(this, &USpectacleRollWidget::HandleVortexPredictedClicked);

	UButton* Meteors = AddModeButton(Canvas, TEXT("MeteorsButton"),
		NSLOCTEXT("SpectacleRollWidget", "MeteorsLabel", "Кубики-метеоры"),
		NSLOCTEXT("SpectacleRollWidget", "MeteorsTooltip",
			"Кубики входят в доску по одному с высоты и сталкиваются с уже упавшими."),
		FLinearColor(0.61f, 0.20f, 0.035f, 0.98f), -14.0f);
	Meteors->OnClicked.AddUniqueDynamic(this, &USpectacleRollWidget::HandleMeteorsClicked);

	UButton* Gravity = AddModeButton(Canvas, TEXT("GravityFlipButton"),
		NSLOCTEXT("SpectacleRollWidget", "GravityLabel", "Переворот гравитации"),
		NSLOCTEXT("SpectacleRollWidget", "GravityTooltip",
			"Кубики всплывают объёмным облаком, а затем одновременно падают."),
		FLinearColor(0.12f, 0.48f, 0.30f, 0.98f), 48.0f);
	Gravity->OnClicked.AddUniqueDynamic(this, &USpectacleRollWidget::HandleGravityFlipClicked);

	UButton* Handful = AddModeButton(Canvas, TEXT("HandfulButton"),
		NSLOCTEXT("SpectacleRollWidget", "HandfulLabel", "Бросок пригоршней"),
		NSLOCTEXT("SpectacleRollWidget", "HandfulTooltip",
			"Кубики слепляются у курсора. Зажмите ЛКМ, сделайте бросковый жест и отпустите."),
		FLinearColor(0.52f, 0.34f, 0.06f, 0.98f), 110.0f);
	Handful->OnClicked.AddUniqueDynamic(this, &USpectacleRollWidget::HandleHandfulClicked);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpectacleStatusText"));
	StatusText->SetText(NSLOCTEXT("SpectacleRollWidget", "Ready",
		"Выберите новый способ броска"));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetAutoWrapText(true);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.91f, 0.96f, 1.0f)));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size = 15;
	StatusText->SetFont(StatusFont);
	UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(StatusText);
	StatusSlot->SetAnchors(FAnchors(1.0f, 0.5f));
	StatusSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	StatusSlot->SetPosition(FVector2D(-24.0f, 146.0f));
	StatusSlot->SetSize(FVector2D(360.0f, 72.0f));
	StatusSlot->SetZOrder(2);
}

UButton* USpectacleRollWidget::AddModeButton(UCanvasPanel* Canvas, const FName Name,
	const FText& Label, const FText& Tooltip, const FLinearColor& Color, const float Y,
	const bool bAnchorLeft)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(Color);
	Button->SetToolTipText(Tooltip);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(Name.ToString() + TEXT("Text"))));
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 18;
	Text->SetFont(Font);
	Button->SetContent(Text);
	UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(Button);
	ButtonSlot->SetAnchors(FAnchors(bAnchorLeft ? 0.0f : 1.0f, 0.5f));
	ButtonSlot->SetAlignment(FVector2D(bAnchorLeft ? 0.0f : 1.0f, 0.5f));
	ButtonSlot->SetPosition(FVector2D(bAnchorLeft ? 24.0f : -24.0f, Y));
	ButtonSlot->SetSize(FVector2D(292.0f, 52.0f));
	ButtonSlot->SetZOrder(2);
	ModeButtons.Add(Button);
	return Button;
}

ASpectacleDiceRollManager* USpectacleRollWidget::FindManager() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ASpectacleDiceRollManager> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool USpectacleRollWidget::IsOtherDiceInteractionActive() const
{
	if (!GetWorld())
	{
		return false;
	}
	for (TActorIterator<ANaturalDiceRollManager> It(GetWorld()); It; ++It)
	{
		if (It->IsRolling())
		{
			return true;
		}
	}
	for (TActorIterator<AMouseGatherDiceManager> It(GetWorld()); It; ++It)
	{
		if (It->IsInteractionActive())
		{
			return true;
		}
	}
	for (TActorIterator<ACPP_Dice> It(GetWorld()); It; ++It)
	{
		const UDicePhysicsRollComponent* Roll = It->FindComponentByClass<UDicePhysicsRollComponent>();
		if (IsValid(Roll) && Roll->IsRolling())
		{
			return true;
		}
	}
	return false;
}

void USpectacleRollWidget::BindManager()
{
	if (IsValid(Manager))
	{
		Manager->OnStatusChanged.AddUniqueDynamic(this, &USpectacleRollWidget::HandleStatusChanged);
	}
}

void USpectacleRollWidget::HandleVortexNaturalClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartVortex(false);
	}
}

void USpectacleRollWidget::HandleVortexPredictedClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartVortex(true);
	}
}

void USpectacleRollWidget::HandleMeteorsClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartMeteors();
	}
}

void USpectacleRollWidget::HandleGravityFlipClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartGravityFlip();
	}
}

void USpectacleRollWidget::HandleHandfulClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartHandful();
	}
}

void USpectacleRollWidget::HandleBackboardNaturalClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartBackboard(false);
	}
}

void USpectacleRollWidget::HandleBackboardPredictedClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartBackboard(true);
	}
}

void USpectacleRollWidget::HandleBackboardDirectedClicked()
{
	if (IsValid(Manager))
	{
		Manager->StartDirectedBackboard();
	}
}

void USpectacleRollWidget::HandleStatusChanged(const FText Status)
{
	if (IsValid(StatusText))
	{
		StatusText->SetText(Status);
	}
}
