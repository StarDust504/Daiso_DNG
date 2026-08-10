// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/AngledDiceHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

/** Назначает отдельный минимальный Widget Blueprint, не затрагивая HUD_Base основной карты. */
AAngledDiceHUD::AAngledDiceHUD()
{
	static ConstructorHelpers::FClassFinder<UUserWidget> RollWidgetFinder(
		TEXT("/Game/Widgets/HUD/W_AngledRollOnly"));
	if (RollWidgetFinder.Succeeded())
	{
		RollWidgetClass = RollWidgetFinder.Class;
	}
}

/** Показывает только кнопку броска и исключает управление костями/камерой с клавиатуры и мыши. */
void AAngledDiceHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !RollWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AngledDiceHUD cannot create W_AngledRollOnly."));
		return;
	}

	RollWidget = CreateWidget<UUserWidget>(PlayerController, RollWidgetClass);
	if (!IsValid(RollWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("AngledDiceHUD failed to create its roll-only widget."));
		return;
	}

	RollWidget->AddToViewport();
	PlayerController->bShowMouseCursor = true;
	// Мировые click/hover-события в тестовой сцене не нужны: UMG-кнопка продолжает получать клики через Slate.
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = false;
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}
