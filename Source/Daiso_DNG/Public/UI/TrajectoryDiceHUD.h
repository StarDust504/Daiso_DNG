// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TrajectoryDiceHUD.generated.h"

class URunStoreWidget;
class UTrajectoryDiceWidget;

/** HUD отдельной игровой сцены с двумя trajectory-бросками и общим магазином забега. */
UCLASS()
class DAISO_DNG_API ATrajectoryDiceHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTrajectoryDiceWidget> GameWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URunStoreWidget> StoreWidget = nullptr;
};
