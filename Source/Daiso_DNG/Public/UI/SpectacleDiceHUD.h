// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NaturalDiceHUD.h"
#include "SpectacleDiceHUD.generated.h"

class USpectacleRollWidget;

/** Adds a native special-roll overlay without modifying the copied existing HUD Blueprint. */
UCLASS()
class DAISO_DNG_API ASpectacleDiceHUD : public ANaturalDiceHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USpectacleRollWidget> SpectacleWidget = nullptr;
};
