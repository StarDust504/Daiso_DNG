// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaturalDiceHUD.generated.h"

class UUserWidget;

/** HUD for the two-button dice physics comparison scene. */
UCLASS()
class DAISO_DNG_API ANaturalDiceHUD : public AHUD
{
	GENERATED_BODY()

public:
	ANaturalDiceHUD();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RollWidget = nullptr;

	UPROPERTY()
	TSubclassOf<UUserWidget> RollWidgetClass;
};
