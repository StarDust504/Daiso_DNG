// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Dice/CPP_Dice.h"
#include "Dice/MouseGatherDiceManager.h"
#include "Dice/SpectacleDiceRollManager.h"
#include "EngineUtils.h"
#include "Game/TrajectoryDiceGameMode.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/GameManagerSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "UI/RunStoreWidget.h"
#include "UI/TrajectoryDiceWidget.h"
#include "UObject/UObjectIterator.h"

namespace TrajectoryDiceSceneTests
{
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FVerifyTrajectorySceneCommand, FAutomationTestBase*, Test);

	bool FVerifyTrajectorySceneCommand::Update()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("Trajectory map created a game world"), World))
		{
			return true;
		}
		Test->TestNotNull(TEXT("Trajectory map uses its isolated GameMode"),
			Cast<ATrajectoryDiceGameMode>(World->GetAuthGameMode()));

		AMouseGatherDiceManager* Gather = nullptr;
		ASpectacleDiceRollManager* Spectacle = nullptr;
		for (TActorIterator<AMouseGatherDiceManager> It(World); It; ++It)
		{
			Gather = *It;
			break;
		}
		for (TActorIterator<ASpectacleDiceRollManager> It(World); It; ++It)
		{
			Spectacle = *It;
			break;
		}
		Test->TestNotNull(TEXT("Manual trajectory gather manager exists"), Gather);
		Test->TestNotNull(TEXT("Automatic handful manager exists"), Spectacle);

		UTrajectoryDiceWidget* GameWidget = nullptr;
		URunStoreWidget* StoreWidget = nullptr;
		for (TObjectIterator<UTrajectoryDiceWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World)
			{
				GameWidget = *It;
				break;
			}
		}
		for (TObjectIterator<URunStoreWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World)
			{
				StoreWidget = *It;
				break;
			}
		}
		Test->TestNotNull(TEXT("Two-mode game HUD exists"), GameWidget);
		Test->TestNotNull(TEXT("Run store fallback exists"), StoreWidget);
		if (IsValid(GameWidget) && GameWidget->WidgetTree)
		{
			Test->TestNotNull(TEXT("HUD has manual gather trajectory button"),
				GameWidget->WidgetTree->FindWidget(TEXT("ManualTrajectoryGatherButton")));
			Test->TestNotNull(TEXT("HUD has automatic gather trajectory button"),
				GameWidget->WidgetTree->FindWidget(TEXT("AutomaticTrajectoryGatherButton")));
			Test->TestNotNull(TEXT("HUD has lower dice result panel"),
				GameWidget->WidgetTree->FindWidget(TEXT("TrajectoryResultsPanel")));
			Test->TestNotNull(TEXT("HUD has finish-round button"),
				GameWidget->WidgetTree->FindWidget(TEXT("TrajectoryFinishRoundButton")));
		}

		UGameManagerSubsystem* Manager = World->GetSubsystem<UGameManagerSubsystem>();
		Test->TestNotNull(TEXT("Scoring/progression subsystem exists"), Manager);
		TArray<ACPP_Dice*> DiceActors;
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			DiceActors.Add(*It);
		}
		DiceActors.Sort([](const ACPP_Dice& Left, const ACPP_Dice& Right)
		{
			return Left.GetName() < Right.GetName();
		});
		Test->TestEqual(TEXT("Trajectory scene keeps six physical dice"), DiceActors.Num(), 6);

		if (IsValid(Manager) && IsValid(GameWidget) && IsValid(Gather)
			&& IsValid(Spectacle) && DiceActors.Num() == 6)
		{
			const TArray<int32> Straight = {1, 2, 3, 4, 5, 6};
			GameWidget->ApplyRollResults(Straight, DiceActors);
			for (ACPP_Dice* Dice : DiceActors)
			{
				Test->TestTrue(TEXT("Auto-selected scoring dice are physically highlighted"),
					Dice->GetIsActive());
				Test->TestFalse(TEXT("Auto-selected scoring dice are locked against rerolls"),
					Dice->GetCanRollDice());
			}

			for (int32 Index = 1; Index < DiceActors.Num(); ++Index)
			{
				Test->TestTrue(TEXT("A physical die can be toggled through the shared click path"),
					GameWidget->TogglePhysicalDiceSelection(DiceActors[Index]));
			}
			ACPP_Dice* HeldDice = DiceActors[0];
			Test->TestTrue(TEXT("The remaining selected die stays registered"),
				Manager->CheckIsDiceRegistered(HeldDice));
			Test->TestFalse(TEXT("The remaining selected die cannot roll"), HeldDice->GetCanRollDice());
			Test->TestTrue(TEXT("An unselected die becomes rollable again"), DiceActors[1]->GetCanRollDice());

			Test->TestTrue(TEXT("Automatic trajectory reroll starts with unselected dice"),
				Spectacle->StartHandful());
			Test->TestEqual(TEXT("Automatic trajectory reroll excludes the held die"),
				Spectacle->GetActiveDiceCount(), 5);
			Spectacle->CancelSpecialRoll();

			Test->TestTrue(TEXT("Manual trajectory gather starts while a die is held"),
				Gather->BeginTrajectoryGather(EMouseGatherDropMode::Natural));
			Test->TestFalse(TEXT("Manual trajectory gather rejects the held die"),
				Gather->TryGatherDice(HeldDice));

			Test->TestTrue(TEXT("A completed physical batch enters scoring atomically"),
				Manager->SetDiceRollSelection(Straight));
			Test->TestEqual(TEXT("The configured straight restores 1500 scoring"),
				Manager->GetSelectedDiceScore().TotalScore, 1500);
			Test->TestTrue(TEXT("A valid result finishes the first round"), Manager->FinishRound());
			Test->TestTrue(TEXT("Finishing a round opens the progression store"), Manager->IsStoreOpen());
		}
		if (IsValid(StoreWidget) && StoreWidget->WidgetTree)
		{
			Test->TestEqual(TEXT("Store overlay becomes visible"), StoreWidget->GetVisibility(),
				ESlateVisibility::Visible);
			Test->TestNotNull(TEXT("Store builds offer cards"),
				StoreWidget->WidgetTree->FindWidget(TEXT("RunStoreOfferCard0")));
			Test->TestNotNull(TEXT("Store has a continue flow"),
				StoreWidget->WidgetTree->FindWidget(TEXT("RunStoreContinueButton")));
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrajectoryDiceSceneConfigurationTest,
	"Daiso.Dice.TrajectoryScene.Configuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrajectoryDiceSceneConfigurationTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Maps/Lvl_Game_TrajectoryThrows"), true))
	{
		AddError(TEXT("Could not open Lvl_Game_TrajectoryThrows."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(
		TrajectoryDiceSceneTests::FVerifyTrajectorySceneCommand(this));
	return true;
}

#endif
