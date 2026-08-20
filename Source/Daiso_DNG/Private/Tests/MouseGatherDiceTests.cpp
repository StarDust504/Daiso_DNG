// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/PrimitiveComponent.h"
#include "Dice/CPP_Dice.h"
#include "Dice/MouseGatherDiceManager.h"
#include "Dice/NaturalDiceRollManager.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UI/NaturalRollWidget.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace MouseGatherDiceTests
{
	constexpr int32 ExpectedDiceCount = 6;

	static UWorld* ResolveGameWorld()
	{
		return AutomationCommon::GetAnyGameWorld();
	}

	static AMouseGatherDiceManager* FindManager(UWorld* World)
	{
		for (TActorIterator<AMouseGatherDiceManager> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	static int32 ReadGeneratedNumber(const AActor* Dice)
	{
		if (!IsValid(Dice))
		{
			return INDEX_NONE;
		}
		const FIntProperty* Property = FindFProperty<FIntProperty>(Dice->GetClass(), TEXT("GeneratedNumber"));
		return Property ? Property->GetPropertyValue_InContainer(Dice) : INDEX_NONE;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
		FBeginGatherCommand, FAutomationTestBase*, Test, EMouseGatherDropMode, Mode);

	bool FBeginGatherCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		AMouseGatherDiceManager* Manager = IsValid(World) ? FindManager(World) : nullptr;
		Test->TestNotNull(TEXT("Mouse gather manager is spawned by NaturalDiceGameMode"), Manager);
		if (!IsValid(Manager))
		{
			return true;
		}

		Test->TestTrue(TEXT("Gather mode starts"), Manager->BeginGather(Mode));
		int32 GatheredCount = 0;
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			if (Manager->TryGatherDice(*It))
			{
				++GatheredCount;
			}
			Test->TestEqual(
				FString::Printf(TEXT("%s has no value while carried"), *It->GetName()),
				ReadGeneratedNumber(*It), 0);
			Test->TestTrue(
				FString::Printf(TEXT("%s remains physical while carried"), *It->GetName()),
				It->SMC_Dice->IsSimulatingPhysics());
		}
		Test->TestEqual(TEXT("Cursor mode gathers all six requested dice"), GatheredCount, ExpectedDiceCount);
		Test->TestEqual(TEXT("Manager tracks the carried group"),
			Manager->GetGatheredDiceCount(), ExpectedDiceCount);
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
		FDropGatheredCommand, FAutomationTestBase*, Test, EMouseGatherDropMode, Mode);

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FVerifyCarriedHeightCommand, FAutomationTestBase*, Test);

	bool FVerifyCarriedHeightCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		int32 RaisedDiceCount = 0;
		TArray<FVector> CarriedLocations;
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			const FVector Location = It->SMC_Dice->GetComponentLocation();
			CarriedLocations.Add(Location);
			if (Location.Z >= 165.0f)
			{
				++RaisedDiceCount;
			}
		}
		Test->TestEqual(TEXT("Every gathered die visibly rises above the board"),
			RaisedDiceCount, ExpectedDiceCount);

		float MinZ = TNumericLimits<float>::Max();
		float MaxZ = -TNumericLimits<float>::Max();
		float MaxPairDistance = 0.0f;
		for (int32 Index = 0; Index < CarriedLocations.Num(); ++Index)
		{
			MinZ = FMath::Min(MinZ, CarriedLocations[Index].Z);
			MaxZ = FMath::Max(MaxZ, CarriedLocations[Index].Z);
			for (int32 OtherIndex = Index + 1; OtherIndex < CarriedLocations.Num(); ++OtherIndex)
			{
				MaxPairDistance = FMath::Max(MaxPairDistance,
					FVector::Distance(CarriedLocations[Index], CarriedLocations[OtherIndex]));
			}
		}
		Test->TestTrue(TEXT("Carried dice form a volumetric group instead of one flat row"),
			MaxZ - MinZ >= 2.0f);
		Test->TestTrue(TEXT("Carried dice stay in a compact cluster"), MaxPairDistance <= 18.0f);
		return true;
	}

	bool FDropGatheredCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		AMouseGatherDiceManager* Manager = IsValid(World) ? FindManager(World) : nullptr;
		if (!IsValid(Manager))
		{
			Test->AddError(TEXT("Drop: mouse gather manager disappeared."));
			return true;
		}
		Test->TestTrue(TEXT("LMB-equivalent release starts the selected drop"), Manager->DropGatheredDice());
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			const int32 ValueAtRelease = ReadGeneratedNumber(*It);
			if (Mode == EMouseGatherDropMode::Natural)
			{
				Test->TestEqual(
					FString::Printf(TEXT("%s remains unknown during an honest fall"), *It->GetName()),
					ValueAtRelease, 0);
			}
			else
			{
				Test->TestTrue(
					FString::Printf(TEXT("%s receives its predicted face at release"), *It->GetName()),
					ValueAtRelease >= 1 && ValueAtRelease <= 6);
			}
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
		FVerifyGatherDropCommand, FAutomationTestBase*, Test, EMouseGatherDropMode, Mode);

	bool FVerifyGatherDropCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		AMouseGatherDiceManager* Manager = IsValid(World) ? FindManager(World) : nullptr;
		Test->TestNotNull(TEXT("Mouse gather manager survives the completed drop"), Manager);
		if (IsValid(Manager))
		{
			Test->TestEqual(TEXT("Manager returns to idle after all dice settle"),
				Manager->GetInteractionState(), EMouseGatherState::Idle);
			if (Mode == EMouseGatherDropMode::Natural)
			{
				Test->TestTrue(TEXT("Honest gathered drop reuses guided impact sound/camera feedback"),
					Manager->GetLastNaturalDropImpactFeedbackCount() > 0);
			}
		}

		int32 DiceCount = 0;
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			++DiceCount;
			const int32 StoredResult = ReadGeneratedNumber(*It);
			float Alignment = 0.0f;
			const int32 PhysicalResult = ANaturalDiceRollManager::DetermineTopFace(
				It->SMC_Dice->GetComponentQuat(), FVector::UpVector, &Alignment);
			Test->TestTrue(
				FString::Printf(TEXT("%s finishes with a valid value"), *It->GetName()),
				StoredResult >= 1 && StoredResult <= 6);
			Test->TestEqual(
				FString::Printf(TEXT("%s value matches the physical top face"), *It->GetName()),
				StoredResult, PhysicalResult);
			Test->TestTrue(
				FString::Printf(TEXT("%s rests clearly on a face"), *It->GetName()), Alignment >= 0.75f);
			Test->TestFalse(
				FString::Printf(TEXT("%s restores its frozen board state"), *It->GetName()),
				It->SMC_Dice->IsSimulatingPhysics());
		}
		Test->TestEqual(TEXT("Completed gather mode verifies all six dice"), DiceCount, ExpectedDiceCount);
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FVerifyGatherWidgetCommand, FAutomationTestBase*, Test);

	bool FVerifyGatherWidgetCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		UNaturalRollWidget* Widget = nullptr;
		for (TObjectIterator<UNaturalRollWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World)
			{
				Widget = *It;
				break;
			}
		}
		Test->TestNotNull(TEXT("Four-mode HUD widget exists"), Widget);
		if (IsValid(Widget) && Widget->WidgetTree)
		{
			Test->TestNotNull(TEXT("HUD contains honest mouse-gather button"),
				Widget->WidgetTree->FindWidget(TEXT("NaturalGatherButton")));
			Test->TestNotNull(TEXT("HUD contains predicted mouse-gather button"),
				Widget->WidgetTree->FindWidget(TEXT("PredictedGatherButton")));
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMouseGatherDiceInteractionTest,
	"Daiso.Dice.MouseGather.Interaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMouseGatherDiceInteractionTest::RunTest(const FString& Parameters)
{
	using namespace MouseGatherDiceTests;
	if (!AutomationOpenMap(TEXT("/Game/Maps/Lvl_Game_AngledRoll"), true))
	{
		AddError(TEXT("Could not open Lvl_Game_AngledRoll for the mouse-gather test."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyGatherWidgetCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FBeginGatherCommand(this, EMouseGatherDropMode::Natural));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyCarriedHeightCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FDropGatheredCommand(this, EMouseGatherDropMode::Natural));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyGatherDropCommand(this, EMouseGatherDropMode::Natural));

	ADD_LATENT_AUTOMATION_COMMAND(FBeginGatherCommand(this, EMouseGatherDropMode::Predicted));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyCarriedHeightCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FDropGatheredCommand(this, EMouseGatherDropMode::Predicted));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyGatherDropCommand(this, EMouseGatherDropMode::Predicted));
	return true;
}

#endif
