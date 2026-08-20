// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TrajectoryDiceHUD.h"

#include "GameFramework/PlayerController.h"
#include "UI/RunStoreWidget.h"
#include "UI/TrajectoryDiceWidget.h"

void ATrajectoryDiceHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	GameWidget = CreateWidget<UTrajectoryDiceWidget>(PlayerController, UTrajectoryDiceWidget::StaticClass());
	StoreWidget = CreateWidget<URunStoreWidget>(PlayerController, URunStoreWidget::StaticClass());
	if (IsValid(GameWidget))
	{
		GameWidget->AddToViewport(10);
	}
	if (IsValid(StoreWidget))
	{
		StoreWidget->AddToViewport(500);
	}

	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = false;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}
