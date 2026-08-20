// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Dice/NaturalDiceRollManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Dice/CPP_Dice.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UI/NaturalRollWidget.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace NaturalDiceRollTests
{
	constexpr int32 ExpectedDiceCount = 6;
	constexpr int32 PhysicalRollCount = 3;

	static UWorld* ResolveGameWorld()
	{
		return AutomationCommon::GetAnyGameWorld();
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

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FStartNaturalRollCommand, FAutomationTestBase*, Test);

	bool FStartNaturalRollCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		if (!IsValid(World))
		{
			Test->AddError(TEXT("Natural roll: game world was not created."));
			return true;
		}

		ANaturalDiceRollManager* Manager = nullptr;
		for (TActorIterator<ANaturalDiceRollManager> It(World); It; ++It)
		{
			Manager = *It;
			break;
		}
		Test->TestNotNull(TEXT("Natural roll manager was spawned by the map game mode"), Manager);
		if (!IsValid(Manager))
		{
			return true;
		}

		UNaturalRollWidget* RollWidget = nullptr;
		for (TObjectIterator<UNaturalRollWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World)
			{
				RollWidget = *It;
				break;
			}
		}
		Test->TestNotNull(TEXT("Natural roll HUD creates its copied widget"), RollWidget);
		UButton* NaturalButton = nullptr;
		if (IsValid(RollWidget) && RollWidget->WidgetTree)
		{
			Test->TestNotNull(TEXT("Copied widget preserves the original GenerateBTN"),
				RollWidget->WidgetTree->FindWidget(TEXT("GenerateBTN")));
			NaturalButton = Cast<UButton>(RollWidget->WidgetTree->FindWidget(TEXT("NaturalRollButton")));
			Test->TestNotNull(TEXT("Copied widget adds the orange natural-roll button"), NaturalButton);
			Test->TestNotNull(TEXT("Copied widget adds a landing-result status line"),
				RollWidget->WidgetTree->FindWidget(TEXT("NaturalRollStatusText")));
		}
		if (IsValid(NaturalButton))
		{
			NaturalButton->OnClicked.Broadcast();
		}
		else
		{
			Test->AddError(TEXT("Natural roll could not be started through its UI button."));
			return true;
		}
		Test->TestTrue(TEXT("Manager reports an active physical roll"), Manager->IsRolling());
		int32 DiceCount = 0;
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			++DiceCount;
			Test->TestEqual(
				FString::Printf(TEXT("%s has no generated value while airborne"), *It->GetName()),
				ReadGeneratedNumber(*It), 0);
		}
		Test->TestEqual(TEXT("Natural roll starts the original six dice"), DiceCount, ExpectedDiceCount);
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FVerifyNaturalRollCommand, FAutomationTestBase*, Test);

	bool FVerifyNaturalRollCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		if (!IsValid(World))
		{
			Test->AddError(TEXT("Natural roll: game world disappeared."));
			return true;
		}

		ANaturalDiceRollManager* Manager = nullptr;
		for (TActorIterator<ANaturalDiceRollManager> It(World); It; ++It)
		{
			Manager = *It;
			break;
		}
		Test->TestNotNull(TEXT("Natural roll manager still exists after settling"), Manager);
		if (IsValid(Manager))
		{
			Test->TestFalse(TEXT("Natural roll completed within its safety window"), Manager->IsRolling());
			Test->TestTrue(TEXT("At least one die has a measurable physical rebound"),
				Manager->GetLastRollReboundCount() > 0);
			Test->TestTrue(TEXT("Honest roll reuses guided impact sound/camera feedback"),
				Manager->GetLastRollImpactFeedbackCount() > 0);
		}

		UPrimitiveComponent* BoardSurface = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!IsValid(*It) || !It->GetClass()->GetName().Contains(TEXT("BP_Board")))
			{
				continue;
			}
			TArray<UPrimitiveComponent*> Components;
			It->GetComponents<UPrimitiveComponent>(Components);
			float LargestArea = -1.0f;
			for (UPrimitiveComponent* Candidate : Components)
			{
				if (!IsValid(Candidate) || Candidate->ComponentHasTag(TEXT("DiceBoundaryWall")))
				{
					continue;
				}
				const FBoxSphereBounds Bounds = Candidate->CalcBounds(FTransform::Identity);
				const float Area = Bounds.BoxExtent.X * Bounds.BoxExtent.Y;
				if (Area > LargestArea)
				{
					LargestArea = Area;
					BoardSurface = Candidate;
				}
			}
			break;
		}
		Test->TestNotNull(TEXT("Visible board surface was resolved for rim checks"), BoardSurface);

		int32 DiceCount = 0;
		for (TActorIterator<ACPP_Dice> It(World); It; ++It)
		{
			++DiceCount;
			const ACPP_Dice* Dice = *It;
			const int32 StoredResult = ReadGeneratedNumber(Dice);
			float Alignment = 0.0f;
			const int32 PhysicalResult = ANaturalDiceRollManager::DetermineTopFace(
				Dice->SMC_Dice->GetComponentQuat(), FVector::UpVector, &Alignment);
			Test->TestTrue(
				FString::Printf(TEXT("%s receives a valid result only after landing"), *Dice->GetName()),
				StoredResult >= 1 && StoredResult <= 6);
			Test->TestEqual(
				FString::Printf(TEXT("%s stored result matches its final physical top face"), *Dice->GetName()),
				StoredResult, PhysicalResult);
			Test->TestTrue(
				FString::Printf(TEXT("%s rests on a clear face"), *Dice->GetName()), Alignment >= 0.75f);
			Test->TestFalse(
				FString::Printf(TEXT("%s restores its original frozen state"), *Dice->GetName()),
				Dice->SMC_Dice->IsSimulatingPhysics());

			if (IsValid(BoardSurface) && IsValid(Manager))
			{
				const FTransform BoardTransform = BoardSurface->GetComponentTransform();
				const FBoxSphereBounds BoardBounds = BoardSurface->CalcBounds(FTransform::Identity);
				const FVector AbsScale = BoardTransform.GetScale3D().GetAbs().ComponentMax(FVector(0.001f));
				const FVector LocalCenter = BoardTransform.InverseTransformPosition(
					Dice->SMC_Dice->GetComponentLocation());
				const float LocalRadiusX = Dice->SMC_Dice->Bounds.BoxExtent.X / AbsScale.X;
				const float LocalRadiusY = Dice->SMC_Dice->Bounds.BoxExtent.Y / AbsScale.Y;
				const float AllowedX = BoardBounds.BoxExtent.X
					- Manager->BoardWallInset / AbsScale.X - LocalRadiusX;
				const float AllowedY = BoardBounds.BoxExtent.Y
					- Manager->BoardWallInset / AbsScale.Y - LocalRadiusY;
				Test->TestTrue(
					FString::Printf(TEXT("%s remains inside the visible X rim"), *Dice->GetName()),
					FMath::Abs(LocalCenter.X - BoardBounds.Origin.X) <= AllowedX + 0.75f);
				Test->TestTrue(
					FString::Printf(TEXT("%s remains inside the visible Y rim"), *Dice->GetName()),
					FMath::Abs(LocalCenter.Y - BoardBounds.Origin.Y) <= AllowedY + 0.75f);
			}
		}
		Test->TestEqual(TEXT("Natural roll verifies the original six dice"), DiceCount, ExpectedDiceCount);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNaturalDiceFaceDetectionTest,
	"Daiso.Dice.NaturalRoll.FaceDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaturalDiceFaceDetectionTest::RunTest(const FString& Parameters)
{
	const TPair<int32, FRotator> LandingRotations[] = {
		{1, FRotator(0.0f, 0.0f, 0.0f)},
		{2, FRotator(0.0f, 0.0f, 90.0f)},
		{3, FRotator(-90.0f, 0.0f, 0.0f)},
		{4, FRotator(90.0f, 0.0f, 0.0f)},
		{5, FRotator(0.0f, 0.0f, -90.0f)},
		{6, FRotator(0.0f, 90.0f, 180.0f)},
	};

	for (const TPair<int32, FRotator>& Entry : LandingRotations)
	{
		float Alignment = 0.0f;
		const int32 Result = ANaturalDiceRollManager::DetermineTopFace(
			Entry.Value.Quaternion(), FVector::UpVector, &Alignment);
		TestEqual(FString::Printf(TEXT("Landing rotation for face %d is detected"), Entry.Key), Result, Entry.Key);
		TestTrue(FString::Printf(TEXT("Face %d points fully upward"), Entry.Key),
			FMath::IsNearlyEqual(Alignment, 1.0f, KINDA_SMALL_NUMBER));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNaturalDicePhysicalRollTest,
	"Daiso.Dice.NaturalRoll.PhysicalOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaturalDicePhysicalRollTest::RunTest(const FString& Parameters)
{
	using namespace NaturalDiceRollTests;
	if (!AutomationOpenMap(TEXT("/Game/Maps/Lvl_Game_AngledRoll"), true))
	{
		AddError(TEXT("Could not open Lvl_Game_AngledRoll for the natural-roll test."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	for (int32 RollIndex = 0; RollIndex < PhysicalRollCount; ++RollIndex)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartNaturalRollCommand(this));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyNaturalRollCommand(this));
	}
	return true;
}

#endif
