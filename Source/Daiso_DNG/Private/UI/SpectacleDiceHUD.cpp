// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SpectacleDiceHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UI/SpectacleRollWidget.h"

void ASpectacleDiceHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}
	SpectacleWidget = CreateWidget<USpectacleRollWidget>(PlayerController, USpectacleRollWidget::StaticClass());
	if (IsValid(SpectacleWidget))
	{
		SpectacleWidget->AddToViewport(20);
	}
}
