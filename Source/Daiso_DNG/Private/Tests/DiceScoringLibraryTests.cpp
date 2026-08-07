// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Dice/DiceScoringLibrary.h"

#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/GameManagerSubsystem.h"

namespace DiceScoringTests
{
	static void AddRule(UDataTable& Table, const FName Name, const EDiceScoringCombinationType Type,
		const int32 Face, const int32 MinCount, const int32 MaxCount, const int32 Start,
		const int32 End, const int32 Score, const EDiceScoreScalingRule Scaling, const int32 Priority)
	{
		FDiceScoringRule Rule;
		Rule.CombinationType = Type;
		Rule.FaceValue = Face;
		Rule.MinDiceCount = MinCount;
		Rule.MaxDiceCount = MaxCount;
		Rule.StraightStart = Start;
		Rule.StraightEnd = End;
		Rule.BaseScore = Score;
		Rule.ScalingRule = Scaling;
		Rule.Priority = Priority;
		Table.AddRow(Name, Rule);
	}

	static UDataTable* MakeKingdomComeRules()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FDiceScoringRule::StaticStruct();
		AddRule(*Table, TEXT("Single_1"), EDiceScoringCombinationType::SingleFace, 1, 1, 1, 1, 6, 100, EDiceScoreScalingRule::None, 10);
		AddRule(*Table, TEXT("Single_5"), EDiceScoringCombinationType::SingleFace, 5, 1, 1, 1, 6, 50, EDiceScoreScalingRule::None, 10);
		const int32 TripleScores[] = {1000, 200, 300, 400, 500, 600};
		for (int32 Face = 1; Face <= 6; ++Face)
		{
			AddRule(*Table, FName(*FString::Printf(TEXT("Same_%d"), Face)), EDiceScoringCombinationType::SameFace,
				Face, 3, 6, 1, 6, TripleScores[Face - 1], EDiceScoreScalingRule::DoublePerAdditionalDie, 20);
		}
		AddRule(*Table, TEXT("Straight_1_5"), EDiceScoringCombinationType::Straight, 1, 5, 5, 1, 5, 500, EDiceScoreScalingRule::None, 30);
		AddRule(*Table, TEXT("Straight_2_6"), EDiceScoringCombinationType::Straight, 1, 5, 5, 2, 6, 750, EDiceScoreScalingRule::None, 30);
		AddRule(*Table, TEXT("Straight_1_6"), EDiceScoringCombinationType::Straight, 1, 6, 6, 1, 6, 1500, EDiceScoreScalingRule::None, 40);
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceScoringExamplesTest,
	"Daiso.Dice.Scoring.KingdomComeExamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiceScoringExamplesTest::RunTest(const FString& Parameters)
{
	UDataTable* Rules = DiceScoringTests::MakeKingdomComeRules();

	struct FCase
	{
		const TCHAR* Name;
		TArray<int32> Dice;
		int32 ExpectedScore;
		int32 ExpectedCombinations;
		bool bExpectedBust;
	};
	const TArray<FCase> Cases = {
		{TEXT("Straight plus spare five"), {1, 2, 3, 4, 5, 5}, 550, 2, false},
		{TEXT("Four ones beat triple plus single"), {1, 1, 1, 1, 5, 2}, 2050, 2, false},
		{TEXT("Full straight"), {1, 2, 3, 4, 5, 6}, 1500, 1, false},
		{TEXT("Six fours"), {4, 4, 4, 4, 4, 4}, 3200, 1, false},
		{TEXT("Five ones plus five"), {1, 1, 1, 1, 1, 5}, 4050, 2, false},
		{TEXT("No scoring dice"), {2, 2, 3, 3, 4, 6}, 0, 0, true}
	};

	for (const FCase& Case : Cases)
	{
		const FDiceRollScoreResult Result = UDiceScoringLibrary::CalculateDiceRollScore(Case.Dice, Rules);
		TestTrue(FString::Printf(TEXT("%s is valid"), Case.Name), Result.bIsValid);
		TestEqual(FString::Printf(TEXT("%s score"), Case.Name), Result.TotalScore, Case.ExpectedScore);
		TestEqual(FString::Printf(TEXT("%s combination count"), Case.Name), Result.Combinations.Num(), Case.ExpectedCombinations);
		TestEqual(FString::Printf(TEXT("%s bust state"), Case.Name), Result.bIsBust, Case.bExpectedBust);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceScoringValidationTest,
	"Daiso.Dice.Scoring.InputValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiceScoringValidationTest::RunTest(const FString& Parameters)
{
	UDataTable* Rules = DiceScoringTests::MakeKingdomComeRules();
	const FDiceRollScoreResult WrongCount = UDiceScoringLibrary::CalculateDiceRollScore({1, 2, 3}, Rules);
	TestFalse(TEXT("A roll must contain exactly six dice"), WrongCount.bIsValid);
	TestFalse(TEXT("Invalid input is not a bust"), WrongCount.bIsBust);

	const FDiceRollScoreResult WrongFace = UDiceScoringLibrary::CalculateDiceRollScore({1, 2, 3, 4, 5, 7}, Rules);
	TestFalse(TEXT("Faces outside 1..6 are rejected"), WrongFace.bIsValid);

	const FDiceRollScoreResult MissingRules = UDiceScoringLibrary::CalculateDiceRollScore({1, 2, 3, 4, 5, 6}, nullptr);
	TestFalse(TEXT("A scoring table is required"), MissingRules.bIsValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceScoringConfiguredAssetTest,
	"Daiso.Dice.Scoring.ConfiguredDataTableAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiceScoringConfiguredAssetTest::RunTest(const FString& Parameters)
{
	UDataTable* Rules = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	if (!TestNotNull(TEXT("The ready-to-use scoring Data Table loads"), Rules))
	{
		return false;
	}
	TestEqual(TEXT("The Data Table contains all prototype rules"), Rules->GetRowMap().Num(), 11);

	const FDiceRollScoreResult Result = UDiceScoringLibrary::CalculateDiceRollScore(
		{1, 2, 3, 4, 5, 5}, Rules);
	TestTrue(TEXT("The imported rules produce a valid result"), Result.bIsValid);
	TestEqual(TEXT("The imported rules score the reference roll"), Result.TotalScore, 550);
	TestEqual(TEXT("The imported rules return both combinations"), Result.Combinations.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceSelectedSubsetTest,
	"Daiso.Dice.Scoring.SelectedSubsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiceSelectedSubsetTest::RunTest(const FString& Parameters)
{
	UDataTable* Rules = DiceScoringTests::MakeKingdomComeRules();

	const FDiceRollScoreResult Singles = UDiceScoringLibrary::CalculateSelectedDiceScore({1, 5}, Rules);
	TestEqual(TEXT("Selected one and five score 150"), Singles.TotalScore, 150);
	TestTrue(TEXT("Both selected scoring dice are consumed"), Singles.bAllDiceScored);

	const FDiceRollScoreResult Mixed = UDiceScoringLibrary::CalculateSelectedDiceScore({1, 2, 2}, Rules);
	TestEqual(TEXT("The scoring part of a mixed selection is still reported"), Mixed.TotalScore, 100);
	TestFalse(TEXT("A selection containing an unscored pair is invalid for keeping"), Mixed.bAllDiceScored);
	TestEqual(TEXT("Both twos remain unscored"), Mixed.UnscoredDiceValues.Num(), 2);

	const FDiceRollScoreResult Triple = UDiceScoringLibrary::CalculateSelectedDiceScore({6, 6, 6}, Rules);
	TestEqual(TEXT("Three selected sixes score 600"), Triple.TotalScore, 600);
	TestTrue(TEXT("A selected triple is fully scoring"), Triple.bAllDiceScored);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiceSelectionSubsystemTest,
	"Daiso.Dice.Scoring.SelectionSubsystemIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiceSelectionSubsystemTest::RunTest(const FString& Parameters)
{
	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->AddComboToTempArray(1);
	Manager->AddComboToTempArray(5);
	TestEqual(TEXT("The existing click backend now scores selected 1 and 5"), Manager->GetCurrentScore(NAME_None), 150);
	TestTrue(TEXT("A fully scoring selection is valid"), Manager->IsCurrentDiceSelectionValid());

	Manager->AddComboToTempArray(2);
	TestEqual(TEXT("Scoring dice are still identified in a mixed selection"), Manager->GetCurrentScore(NAME_None), 150);
	TestFalse(TEXT("A selected non-scoring die makes the selection invalid"), Manager->IsCurrentDiceSelectionValid());

	Manager->ClearDiceSelection();
	Manager->AddComboToTempArray(2);
	Manager->AddComboToTempArray(2);
	Manager->AddComboToTempArray(2);
	TestEqual(TEXT("Completing the selected pair into a triple scores it"), Manager->GetCurrentScore(NAME_None), 200);
	TestTrue(TEXT("The completed selected triple is valid"), Manager->IsCurrentDiceSelectionValid());
	return true;
}

#endif
