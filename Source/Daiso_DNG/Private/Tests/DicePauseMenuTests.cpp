// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Game/NaturalDiceGameMode.h"
#include "Player/DiceRollPlayerController.h"
#include "UI/DicePauseMenuWidget.h"
#include "UObject/UObjectIterator.h"

namespace DicePauseMenuTests
{
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FVerifyPauseMenuCommand, FAutomationTestBase*, Test);

	bool FVerifyPauseMenuCommand::Update()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		ADiceRollPlayerController* Controller = IsValid(World)
			? Cast<ADiceRollPlayerController>(World->GetFirstPlayerController())
			: nullptr;
		Test->TestNotNull(TEXT("Roll map creates the ESC-menu player controller"), Controller);
		if (!IsValid(Controller))
		{
			return true;
		}

		bool bHasEscapeBinding = false;
		bool bEscapeWorksWhilePaused = false;
		if (Controller->InputComponent)
		{
			for (const FInputKeyBinding& Binding : Controller->InputComponent->KeyBindings)
			{
				if (Binding.Chord.Key == EKeys::Escape && Binding.KeyEvent == IE_Pressed)
				{
					bHasEscapeBinding = true;
					bEscapeWorksWhilePaused = Binding.bExecuteWhenPaused;
					break;
				}
			}
		}
		Test->TestTrue(TEXT("ESC is bound to the pause-menu toggle"), bHasEscapeBinding);
		Test->TestTrue(TEXT("ESC remains active while the world is paused"), bEscapeWorksWhilePaused);

		Controller->TogglePauseMenu();
		Test->TestTrue(TEXT("First ESC-equivalent opens the pause menu"), Controller->IsPauseMenuOpen());
		Test->TestTrue(TEXT("Opening the menu pauses dice physics"), UGameplayStatics::IsGamePaused(World));

		UDicePauseMenuWidget* PauseMenu = nullptr;
		UButton* ContinueButton = nullptr;
		for (TObjectIterator<UDicePauseMenuWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World)
			{
				PauseMenu = *It;
				break;
			}
		}
		Test->TestNotNull(TEXT("Pause menu widget is created on demand"), PauseMenu);
		if (IsValid(PauseMenu) && PauseMenu->WidgetTree)
		{
			ContinueButton = Cast<UButton>(PauseMenu->WidgetTree->FindWidget(TEXT("ContinueButton")));
			Test->TestNotNull(TEXT("Pause menu contains Continue"), ContinueButton);
			Test->TestNotNull(TEXT("Pause menu contains Quit"),
				PauseMenu->WidgetTree->FindWidget(TEXT("QuitButton")));
		}

		Controller->TogglePauseMenu();
		Test->TestFalse(TEXT("Second ESC-equivalent closes the pause menu"), Controller->IsPauseMenuOpen());
		Test->TestFalse(TEXT("Closing the menu resumes dice physics"), UGameplayStatics::IsGamePaused(World));

		Controller->TogglePauseMenu();
		Test->TestTrue(TEXT("Menu reopens for the Continue-button check"), Controller->IsPauseMenuOpen());
		if (IsValid(ContinueButton))
		{
			ContinueButton->OnClicked.Broadcast();
		}
		Test->TestFalse(TEXT("Continue closes the pause menu"), Controller->IsPauseMenuOpen());
		Test->TestFalse(TEXT("Continue resumes dice physics"), UGameplayStatics::IsGamePaused(World));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDicePauseMenuConfigurationTest,
	"Daiso.Dice.PauseMenu.Configuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDicePauseMenuConfigurationTest::RunTest(const FString& Parameters)
{
	const ANaturalDiceGameMode* GameMode = GetDefault<ANaturalDiceGameMode>();
	TestNotNull(TEXT("Natural dice game mode CDO exists"), GameMode);
	if (GameMode)
	{
		TestEqual(TEXT("Natural dice map uses the ESC-menu player controller"),
			GameMode->PlayerControllerClass.Get(), ADiceRollPlayerController::StaticClass());
	}
	TestTrue(TEXT("Pause menu remains a standalone native UserWidget"),
		UDicePauseMenuWidget::StaticClass()->IsChildOf(UUserWidget::StaticClass()));
	if (!AutomationOpenMap(TEXT("/Game/Maps/Lvl_Game_AngledRoll"), true))
	{
		AddError(TEXT("Could not open the isolated dice-roll map for the pause-menu test."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(DicePauseMenuTests::FVerifyPauseMenuCommand(this));
	return true;
}

#endif
