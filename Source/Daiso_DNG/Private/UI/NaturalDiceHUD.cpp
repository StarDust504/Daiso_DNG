// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/NaturalDiceHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

ANaturalDiceHUD::ANaturalDiceHUD()
{
}

void ANaturalDiceHUD::BeginPlay()
{
	Super::BeginPlay();
	if (!RollWidgetClass)
	{
		// Resolve at runtime so a clean project can load this native class before the editor setup script
		// creates the copied Widget Blueprint for the first time.
		RollWidgetClass = LoadClass<UUserWidget>(
			nullptr, TEXT("/Game/Widgets/HUD/W_NaturalRollOnly.W_NaturalRollOnly_C"));
	}
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !RollWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("NaturalDiceHUD cannot create W_NaturalRollOnly."));
		return;
	}

	RollWidget = CreateWidget<UUserWidget>(PlayerController, RollWidgetClass);
	if (!IsValid(RollWidget))
	{
		return;
	}
	RollWidget->AddToViewport();
	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = false;
	// Game-and-UI keeps UMG buttons active while also feeding LMB and cursor position to the world gather manager.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}
