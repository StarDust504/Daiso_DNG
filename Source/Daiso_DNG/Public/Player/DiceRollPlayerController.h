// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DiceRollPlayerController.generated.h"

class UDicePauseMenuWidget;

/** Player controller for the isolated dice-roll map, including its ESC pause menu. */
UCLASS()
class DAISO_DNG_API ADiceRollPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;

	UFUNCTION()
	void TogglePauseMenu();

	UFUNCTION()
	void ClosePauseMenu();

	UFUNCTION()
	void QuitFromPauseMenu();

	UFUNCTION(BlueprintPure)
	bool IsPauseMenuOpen() const;

private:
	void OpenPauseMenu();
	void ApplyRollMapInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UDicePauseMenuWidget> PauseMenuWidget = nullptr;
};
