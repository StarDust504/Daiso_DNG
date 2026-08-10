// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PrimitiveComponent.h"
#include "Dice/CPP_Dice.h"
#include "Dice/DicePhysicsRollComponent.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace DicePhysicsContainmentTests
{
	constexpr int32 ExpectedDiceCount = 6;
	constexpr int32 StressRollCount = 8;
	constexpr float SecondsPerRoll = 4.0f;
	constexpr float ExpectedPlayableSurfaceInset = 2.8f;

	/** Возвращает игровой мир, который AutomationOpenMap создаёт для PIE-прогона. */
	static UWorld* ResolveGameWorld()
	{
		return AutomationCommon::GetAnyGameWorld();
	}

	/** Определяет шесть рабочих BP_Dice_Basic, не привязывая тест к случайным instance-именам. */
	static bool IsBasicDice(const AActor* Actor)
	{
		return IsValid(Actor) && Actor->GetClass()->GetName().Contains(TEXT("BP_Dice_Basic"));
	}

	/**
	 * Проверяет чистое стартовое состояние тестовой сцены до первого нажатия кнопки.
	 * Это защищает сцену от унаследованного Blueprint BeginPlay, который раньше мог
	 * самопроизвольно бросить и подсветить сразу все кубики при загрузке уровня.
	 */
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
		FVerifyInitialDiceIdleCommand, FAutomationTestBase*, Test);

	bool FVerifyInitialDiceIdleCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		if (!IsValid(World))
		{
			Test->AddError(TEXT("Initial state: game world was not created."));
			return true;
		}

		int32 CheckedDiceCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* DiceActor = *It;
			if (!IsBasicDice(DiceActor))
			{
				continue;
			}

			++CheckedDiceCount;
			ACPP_Dice* Dice = Cast<ACPP_Dice>(DiceActor);
			const UDicePhysicsRollComponent* RollComponent =
				DiceActor->FindComponentByClass<UDicePhysicsRollComponent>();

			Test->TestNotNull(
				FString::Printf(TEXT("%s has a physics roll component"), *DiceActor->GetName()),
				RollComponent);
			if (RollComponent != nullptr)
			{
				Test->TestFalse(
					FString::Printf(TEXT("%s does not roll before the button is pressed"), *DiceActor->GetName()),
					RollComponent->IsRolling());
			}

			Test->TestNotNull(
				FString::Printf(TEXT("%s derives from ACPP_Dice"), *DiceActor->GetName()),
				Dice);
			if (Dice != nullptr)
			{
				Test->TestFalse(
					FString::Printf(TEXT("%s is not highlighted at scene start"), *DiceActor->GetName()),
					Dice->GetIsActive());
			}
		}

		Test->TestEqual(TEXT("Initial state verifies every dice"), CheckedDiceCount, ExpectedDiceCount);
		return true;
	}

	/** Вызывает тот же Blueprint RollDice, который вызывает UI-кнопка, у всех кубиков карты. */
	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
		FRollAllDiceCommand, FAutomationTestBase*, Test, int32, RollIndex);

	bool FRollAllDiceCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		if (!IsValid(World))
		{
			Test->AddError(FString::Printf(TEXT("Roll %d: game world was not created."), RollIndex));
			return true;
		}

		int32 StartedDiceCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Dice = *It;
			if (!IsBasicDice(Dice))
			{
				continue;
			}

			if (UFunction* RollFunction = Dice->FindFunction(TEXT("RollDice"));
				IsValid(RollFunction) && RollFunction->ParmsSize == 0)
			{
				Dice->ProcessEvent(RollFunction, nullptr);
				++StartedDiceCount;
			}
		}

		Test->TestEqual(
			FString::Printf(TEXT("Roll %d starts every dice"), RollIndex),
			StartedDiceCount, ExpectedDiceCount);
		return true;
	}

	/** Находит верх меша доски и проверяет, что после завершения броска ни один кубик не находится под её защитным полом. */
	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
		FVerifyDiceContainmentCommand, FAutomationTestBase*, Test, int32, RollIndex);

	bool FVerifyDiceContainmentCommand::Update()
	{
		UWorld* World = ResolveGameWorld();
		if (!IsValid(World))
		{
			Test->AddError(FString::Printf(TEXT("Roll %d: game world disappeared."), RollIndex));
			return true;
		}

		float BoardTop = -TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Candidate = *It;
			if (!IsValid(Candidate) || !Candidate->GetClass()->GetName().Contains(TEXT("BP_Board")))
			{
				continue;
			}

			TArray<UPrimitiveComponent*> BoardComponents;
			Candidate->GetComponents<UPrimitiveComponent>(BoardComponents);
			float LargestArea = -1.0f;
			for (UPrimitiveComponent* Component : BoardComponents)
			{
				if (!IsValid(Component) || Component->ComponentHasTag(TEXT("DiceBoundaryWall")))
				{
					continue;
				}
				const float Area = Component->Bounds.BoxExtent.X * Component->Bounds.BoxExtent.Y;
				if (Area > LargestArea)
				{
					LargestArea = Area;
					BoardTop = Component->Bounds.Origin.Z + Component->Bounds.BoxExtent.Z;
				}
			}
			break;
		}

		BoardTop -= ExpectedPlayableSurfaceInset;
		Test->TestTrue(TEXT("Playable board surface was resolved"), FMath::IsFinite(BoardTop));
		int32 CheckedDiceCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Dice = *It;
			if (!IsBasicDice(Dice))
			{
				continue;
			}

			++CheckedDiceCount;
			const float DiceZ = Dice->GetActorLocation().Z;
			float DiceBottom = DiceZ;
			if (const UPrimitiveComponent* DiceBody = Cast<UPrimitiveComponent>(Dice->GetRootComponent()))
			{
				DiceBottom -= DiceBody->Bounds.BoxExtent.Z;
			}
			Test->TestTrue(
				FString::Printf(TEXT("Roll %d keeps %s above the board floor (Z %.2f, top %.2f)"),
					RollIndex, *Dice->GetName(), DiceZ, BoardTop),
				FMath::IsFinite(DiceZ) && DiceZ >= BoardTop - 12.0f);
			Test->TestTrue(
				FString::Printf(TEXT("Roll %d rests %s on the playable surface (bottom %.2f, surface %.2f)"),
					RollIndex, *Dice->GetName(), DiceBottom, BoardTop),
				FMath::IsFinite(DiceBottom) && FMath::Abs(DiceBottom - BoardTop) <= 0.75f);
		}
		Test->TestEqual(
			FString::Printf(TEXT("Roll %d verifies every dice"), RollIndex),
			CheckedDiceCount, ExpectedDiceCount);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDicePhysicsBoardContainmentTest,
	"Daiso.Dice.Physics.BoardContainment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDicePhysicsBoardContainmentTest::RunTest(const FString& Parameters)
{
	using namespace DicePhysicsContainmentTests;
	if (!AutomationOpenMap(TEXT("/Game/Maps/Lvl_Game_AngledRoll"), true))
	{
		AddError(TEXT("Could not open Lvl_Game_AngledRoll for the containment stress test."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyInitialDiceIdleCommand(this));
	for (int32 RollIndex = 1; RollIndex <= StressRollCount; ++RollIndex)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRollAllDiceCommand(this, RollIndex));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(SecondsPerRoll));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyDiceContainmentCommand(this, RollIndex));
	}
	return true;
}

#endif
