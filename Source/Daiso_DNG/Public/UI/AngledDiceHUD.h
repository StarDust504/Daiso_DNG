// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AngledDiceHUD.generated.h"

class UUserWidget;

/** HUD отдельной angled-сцены, создающий только минимальный экран кнопки броска. */
UCLASS()
class DAISO_DNG_API AAngledDiceHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Находит класс W_AngledRollOnly при создании CDO. */
	AAngledDiceHUD();

protected:
	/** Создаёт один виджет, показывает курсор и переводит контроллер в режим UI Only. */
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RollWidget = nullptr;

	UPROPERTY()
	TSubclassOf<UUserWidget> RollWidgetClass;
};

