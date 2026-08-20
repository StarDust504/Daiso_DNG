// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/DiceRollPlayerController.h"

#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/DicePauseMenuWidget.h"

void ADiceRollPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		FInputKeyBinding& EscapeBinding = InputComponent->BindKey(
			EKeys::Escape, IE_Pressed, this, &ADiceRollPlayerController::TogglePauseMenu);
		EscapeBinding.bExecuteWhenPaused = true;
	}
}

void ADiceRollPlayerController::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void ADiceRollPlayerController::OpenPauseMenu()
{
	if (!IsValid(PauseMenuWidget))
	{
		PauseMenuWidget = CreateWidget<UDicePauseMenuWidget>(this, UDicePauseMenuWidget::StaticClass());
		if (!IsValid(PauseMenuWidget))
		{
			UE_LOG(LogTemp, Error, TEXT("DiceRollPlayerController could not create the pause menu."));
			return;
		}
		PauseMenuWidget->AddToViewport(1000);
	}
	else
	{
		PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}

	SetPause(true);
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	PauseMenuWidget->SetKeyboardFocus();
}

void ADiceRollPlayerController::ClosePauseMenu()
{
	if (!IsPauseMenuOpen())
	{
		return;
	}

	SetPause(false);
	PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	ApplyRollMapInputMode();
}

void ADiceRollPlayerController::QuitFromPauseMenu()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

bool ADiceRollPlayerController::IsPauseMenuOpen() const
{
	return IsValid(PauseMenuWidget)
		&& PauseMenuWidget->GetVisibility() != ESlateVisibility::Collapsed
		&& PauseMenuWidget->GetVisibility() != ESlateVisibility::Hidden;
}

void ADiceRollPlayerController::ApplyRollMapInputMode()
{
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}
