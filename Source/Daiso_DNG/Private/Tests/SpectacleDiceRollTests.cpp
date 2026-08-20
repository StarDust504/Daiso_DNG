// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Dice/CPP_Dice.h"
#include "Dice/SpectacleDiceRollManager.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UI/SpectacleRollWidget.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace SpectacleDiceRollTests
{
	static int32 ReadGeneratedNumber(const AActor* Dice)
	{
		const FIntProperty* Property = IsValid(Dice)
			? FindFProperty<FIntProperty>(Dice->GetClass(), TEXT("GeneratedNumber"))
			: nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Dice) : INDEX_NONE;
	}

	static ASpectacleDiceRollManager* FindManager(UWorld* World)
	{
		for (TActorIterator<ASpectacleDiceRollManager> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifySpectacleModesCommand, FAutomationTestBase*, Test);

	bool FVerifySpectacleModesCommand::Update()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		ASpectacleDiceRollManager* Manager = IsValid(World) ? FindManager(World) : nullptr;
		Test->TestNotNull(TEXT("Isolated roll map spawns the spectacle manager"), Manager);
		if (!IsValid(Manager))
		{
			return true;
		}

		USpectacleRollWidget* Widget = nullptr;
		for (TObjectIterator<USpectacleRollWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World)
			{
				Widget = *It;
				break;
			}
		}
		Test->TestNotNull(TEXT("Spectacle HUD overlay exists only on the roll map"), Widget);
		if (IsValid(Widget) && Widget->WidgetTree)
		{
			Test->TestNotNull(TEXT("HUD contains honest vortex button"),
				Widget->WidgetTree->FindWidget(TEXT("VortexNaturalButton")));
			Test->TestNotNull(TEXT("HUD contains predicted vortex button"),
				Widget->WidgetTree->FindWidget(TEXT("VortexPredictedButton")));
			Test->TestNotNull(TEXT("HUD contains meteor button"),
				Widget->WidgetTree->FindWidget(TEXT("MeteorsButton")));
			Test->TestNotNull(TEXT("HUD contains gravity-flip button"),
				Widget->WidgetTree->FindWidget(TEXT("GravityFlipButton")));
			Test->TestNotNull(TEXT("HUD contains handful button"),
				Widget->WidgetTree->FindWidget(TEXT("HandfulButton")));
			Test->TestNotNull(TEXT("HUD contains automatic honest backboard button"),
				Widget->WidgetTree->FindWidget(TEXT("BackboardNaturalButton")));
			Test->TestNotNull(TEXT("HUD contains automatic predicted backboard button"),
				Widget->WidgetTree->FindWidget(TEXT("BackboardPredictedButton")));
			Test->TestNotNull(TEXT("HUD contains LMB-directed backboard button"),
				Widget->WidgetTree->FindWidget(TEXT("BackboardDirectedButton")));
		}

		FAutomationTestBase* ActiveTest = Test;
		UWorld* ActiveWorld = World;
		auto VerifyUnknownValues = [ActiveTest, ActiveWorld]()
		{
			int32 DiceCount = 0;
			for (TActorIterator<ACPP_Dice> It(ActiveWorld); It; ++It)
			{
				++DiceCount;
				ActiveTest->TestEqual(FString::Printf(TEXT("%s has no pre-generated honest result"), *It->GetName()),
					ReadGeneratedNumber(*It), 0);
			}
			ActiveTest->TestEqual(TEXT("Every special mode owns all six map dice"), DiceCount, 6);
		};

		Test->TestTrue(TEXT("Honest vortex starts"), Manager->StartVortex(false));
		Test->TestEqual(TEXT("Honest vortex enters physical orbit phase"),
			Manager->GetPhase(), ESpectacleDicePhase::Vortex);
		VerifyUnknownValues();
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("Predicted vortex starts"), Manager->StartVortex(true));
		Test->TestEqual(TEXT("Prediction is not selected before vortex release"),
			Manager->GetPhase(), ESpectacleDicePhase::Vortex);
		VerifyUnknownValues();
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("Meteor cascade starts"), Manager->StartMeteors());
		Test->TestEqual(TEXT("Meteor cascade enters staggered drop phase"),
			Manager->GetPhase(), ESpectacleDicePhase::MeteorDrop);
		VerifyUnknownValues();
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("Gravity flip starts"), Manager->StartGravityFlip());
		Test->TestEqual(TEXT("Gravity flip enters floating-cloud phase"),
			Manager->GetPhase(), ESpectacleDicePhase::GravityLift);
		VerifyUnknownValues();
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("Handful mode starts"), Manager->StartHandful());
		Test->TestEqual(TEXT("Handful waits for a separate mouse gesture"),
			Manager->GetPhase(), ESpectacleDicePhase::HandfulAiming);
		VerifyUnknownValues();
		Test->TestTrue(TEXT("Handful can be released into a real physical throw"), Manager->ReleaseHandful());
		Test->TestEqual(TEXT("Released handful uses honest post-landing result phase"),
			Manager->GetPhase(), ESpectacleDicePhase::NaturalSettling);
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("Automatic honest throw from behind the board starts"),
			Manager->StartBackboard(false));
		Test->TestEqual(TEXT("Automatic honest backboard throw enters natural settling"),
			Manager->GetPhase(), ESpectacleDicePhase::NaturalSettling);
		Test->TestEqual(TEXT("Automatic honest backboard mode is reported"),
			Manager->GetMode(), ESpectacleDiceMode::BackboardNatural);
		VerifyUnknownValues();
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("Automatic predicted throw from behind the board starts"),
			Manager->StartBackboard(true));
		Test->TestEqual(TEXT("Automatic predicted backboard throw enters guided settling"),
			Manager->GetPhase(), ESpectacleDicePhase::PredictedSettling);
		Test->TestEqual(TEXT("Automatic predicted backboard mode is reported"),
			Manager->GetMode(), ESpectacleDiceMode::BackboardPredicted);
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			const int32 Result = ReadGeneratedNumber(*It);
			Test->TestTrue(FString::Printf(TEXT("%s receives a predicted backboard result"), *It->GetName()),
				Result >= 1 && Result <= 6);
		}
		Manager->CancelSpecialRoll();

		Test->TestTrue(TEXT("LMB-directed throw from behind the board can be armed"),
			Manager->StartDirectedBackboard());
		Test->TestEqual(TEXT("Directed backboard throw waits for a separate map click"),
			Manager->GetPhase(), ESpectacleDicePhase::BackboardAiming);
		VerifyUnknownValues();
		Test->TestTrue(TEXT("Directed backboard throw can be released toward the board"),
			Manager->ReleaseDirectedBackboard());
		Test->TestEqual(TEXT("Directed backboard throw keeps an honest physical result"),
			Manager->GetPhase(), ESpectacleDicePhase::NaturalSettling);
		Manager->CancelSpecialRoll();

		Test->TestFalse(TEXT("Manager returns to idle after restoring every die"), Manager->IsActive());
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			Test->TestFalse(FString::Printf(TEXT("%s restores the map's frozen state"), *It->GetName()),
				It->SMC_Dice->IsSimulatingPhysics());
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpectacleDiceRollConfigurationTest,
	"Daiso.Dice.Spectacle.Configuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSpectacleDiceRollConfigurationTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Maps/Lvl_Game_AngledRoll"), true))
	{
		AddError(TEXT("Could not open the isolated dice-roll map for the spectacle test."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(SpectacleDiceRollTests::FVerifySpectacleModesCommand(this));
	return true;
}

#endif
