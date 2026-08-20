// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DicePauseMenuWidget.generated.h"

class UButton;
class UVerticalBox;

/** Native pause menu used only by the isolated dice-roll map. */
UCLASS()
class DAISO_DNG_API UDicePauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();
	UButton* AddMenuButton(UVerticalBox* Container, FName Name, const FText& Label,
		const FLinearColor& Color);

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> ContinueButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton = nullptr;
};
