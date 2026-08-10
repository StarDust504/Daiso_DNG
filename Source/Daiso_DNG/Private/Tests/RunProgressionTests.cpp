// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataTable.h"
#include "Progression/BoostTypes.h"
#include "Progression/LevelProgressTypes.h"
#include "Subsystems/GameManagerSubsystem.h"

namespace RunProgressionTests
{
	/** Создаёт пустую in-memory таблицу этапов с правильным RowStruct. */
	static UDataTable* MakeStagesTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FRunStageRow::StaticStruct();
		return Table;
	}

	/** Добавляет валидную строку этапа с явно заданной экономикой и весами магазина. */
	static void AddStage(UDataTable& Table, const int32 Round, const int32 RequiredInvoice,
		const int32 Win, const int32 Lose, const int32 Store,
		const float Common = 1.0f, const float Rare = 0.0f,
		const float Epic = 0.0f, const float Legendary = 0.0f)
	{
		FRunStageRow Stage;
		Stage.Round = Round;
		Stage.RequiredInvoice = RequiredInvoice;
		Stage.Win = Win;
		Stage.Lose = Lose;
		Stage.Store = Store;
		Stage.Common = Common;
		Stage.Rare = Rare;
		Stage.Epic = Epic;
		Stage.Legendary = Legendary;
		Table.AddRow(FName(*FString::Printf(TEXT("Round_%02d"), Round)), Stage);
	}

	/** Создаёт пустую in-memory таблицу бустов с правильным RowStruct. */
	static UDataTable* MakeBoostsTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FBoostRow::StaticStruct();
		return Table;
	}

	/** Добавляет компактную строку буста, пригодную для магазина и effect-тестов. */
	static void AddBoost(UDataTable& Table, const FName Id, const EBoostRarity Rarity,
		const int32 Cost, const int32 MaxStacks, const EBoostEffectTrigger Trigger = EBoostEffectTrigger::None,
		const EBoostEffectOperation Operation = EBoostEffectOperation::None,
		const float Magnitude = 0.0f, const float Threshold = 0.0f, const int32 Face = 0)
	{
		FBoostRow Boost;
		Boost.DisplayName = FText::FromName(Id);
		Boost.Rarity = Rarity;
		Boost.Cost = Cost;
		Boost.EffectDescription = FText::FromString(FString::Printf(TEXT("Effect %s"), *Id.ToString()));
		Boost.MaxStacks = MaxStacks;
		Boost.EffectTrigger = Trigger;
		Boost.EffectOperation = Operation;
		Boost.EffectMagnitude = Magnitude;
		Boost.EffectThreshold = Threshold;
		Boost.EffectFaceValue = Face;
		Table.AddRow(Id, Boost);
	}

	/** Выбирает заданные грани через неизменённый публичный click/scoring путь подсистемы. */
	static void SelectDice(UGameManagerSubsystem& Manager, const TArray<int32>& Faces)
	{
		for (const int32 Face : Faces)
		{
			Manager.AddComboToTempArray(Face);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionConfiguredAssetsTest,
	"Daiso.RunProgression.ConfiguredDataTables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Проверяет число строк и ключевые значения импортированных листов Balance_dice.xlsx. */
bool FRunProgressionConfiguredAssetsTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_RunStages.DT_RunStages"));
	UDataTable* Boosts = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Boosts.DT_Boosts"));
	if (!TestNotNull(TEXT("DT_RunStages loads"), Stages)
		|| !TestNotNull(TEXT("DT_Boosts loads"), Boosts))
	{
		return false;
	}

	TestEqual(TEXT("DT_RunStages contains all eight rounds"), Stages->GetRowMap().Num(), 8);
	TestEqual(TEXT("DT_Boosts contains all thirty-four boosts"), Boosts->GetRowMap().Num(), 34);
	const FRunStageRow* FirstStage = Stages->FindRow<FRunStageRow>(
		TEXT("Round_01"), TEXT("FRunProgressionConfiguredAssetsTest"), false);
	const FRunStageRow* LastStage = Stages->FindRow<FRunStageRow>(
		TEXT("Round_08"), TEXT("FRunProgressionConfiguredAssetsTest"), false);
	if (TestNotNull(TEXT("Round one exists"), FirstStage))
	{
		TestEqual(TEXT("Round one invoice"), FirstStage->RequiredInvoice, 1500);
		TestEqual(TEXT("Round one win economy"), FirstStage->Win, 7);
		TestEqual(TEXT("Round one lose economy"), FirstStage->Lose, -4);
		TestEqual(TEXT("Round one store size"), FirstStage->Store, 3);
	}
	if (TestNotNull(TEXT("Round eight exists"), LastStage))
	{
		TestEqual(TEXT("Round eight invoice"), LastStage->RequiredInvoice, 20000000);
		TestEqual(TEXT("Round eight store size"), LastStage->Store, 4);
	}

	const FBoostRow* BaseBoost = Boosts->FindRow<FBoostRow>(
		TEXT("C01"), TEXT("FRunProgressionConfiguredAssetsTest"), false);
	const FBoostRow* FarkleBoost = Boosts->FindRow<FBoostRow>(
		TEXT("C08"), TEXT("FRunProgressionConfiguredAssetsTest"), false);
	const FBoostRow* XMultBoost = Boosts->FindRow<FBoostRow>(
		TEXT("E01"), TEXT("FRunProgressionConfiguredAssetsTest"), false);
	if (TestNotNull(TEXT("C01 imported effect exists"), BaseBoost))
	{
		TestEqual(TEXT("C01 cost comes from the workbook"), BaseBoost->Cost, 4);
		TestEqual(TEXT("C01 MaxStacks comes from the workbook"), BaseBoost->MaxStacks, 3);
		TestEqual(TEXT("C01 uses the Base operation"),
			BaseBoost->EffectOperation, EBoostEffectOperation::AddBasePerMatchingDie);
		TestEqual(TEXT("C01 Base magnitude is imported"), BaseBoost->EffectMagnitude, 100.0f);
	}
	if (TestNotNull(TEXT("C08 imported effect exists"), FarkleBoost))
	{
		TestEqual(TEXT("C08 uses the Farkle trigger"),
			FarkleBoost->EffectTrigger, EBoostEffectTrigger::Farkle);
		TestEqual(TEXT("C08 preserves half of the turn"), FarkleBoost->EffectMagnitude, 0.5f);
	}
	if (TestNotNull(TEXT("E01 imported effect exists"), XMultBoost))
	{
		TestEqual(TEXT("E01 uses the XMult operation"),
			XMultBoost->EffectOperation, EBoostEffectOperation::MultiplyScore);
		TestEqual(TEXT("E01 requires Mult ten"), XMultBoost->EffectThreshold, 10.0f);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionEconomyOutcomeTest,
	"Daiso.RunProgression.Economy.WinLoseAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Проверяет стартовые 25 монет, Win/Lose, отложенный переход и повтор цели после поражения. */
bool FRunProgressionEconomyOutcomeTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = RunProgressionTests::MakeStagesTable();
	RunProgressionTests::AddStage(*Stages, 1, 1500, 7, -4, 0);
	RunProgressionTests::AddStage(*Stages, 2, 4000, 8, -5, 0);
	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->RegisterRunDataTables(Stages, RunProgressionTests::MakeBoostsTable());

	TestEqual(TEXT("A new run starts with 25 money"), Manager->GetMoney(), 25);
	RunProgressionTests::SelectDice(*Manager, {1, 2, 3, 4, 5, 6});
	TestTrue(TEXT("The winning round finishes"), Manager->FinishRound());
	TestEqual(TEXT("Win adds the configured seven money"), Manager->GetMoney(), 32);
	TestTrue(TEXT("Win opens the mandatory store phase"), Manager->IsStoreOpen());
	TestEqual(TEXT("The old target stays active until store close"),
		Manager->GetLevelProgress().LevelNumber, 1);
	TestTrue(TEXT("The win store closes"), Manager->CloseStore());
	TestEqual(TEXT("Closing a win store advances to round two"),
		Manager->GetLevelProgress().LevelNumber, 2);

	RunProgressionTests::SelectDice(*Manager, {1, 2, 3, 4, 5, 6});
	TestTrue(TEXT("A valid insufficient round finishes"), Manager->FinishRound());
	TestEqual(TEXT("Lose subtracts the configured five money"), Manager->GetMoney(), 27);
	TestTrue(TEXT("Lose also opens the mandatory store phase"), Manager->IsStoreOpen());
	TestTrue(TEXT("The lose store closes"), Manager->CloseStore());
	TestEqual(TEXT("Closing a lose store repeats round two"),
		Manager->GetLevelProgress().LevelNumber, 2);
	TestEqual(TEXT("The repeated target is unchanged"),
		Manager->GetLevelProgress().TargetScore, 4000);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionGameOverTest,
	"Daiso.RunProgression.Economy.GameOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Проверяет Game Over при нулевом балансе и запрет запуска ещё одного игрового раунда. */
bool FRunProgressionGameOverTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = RunProgressionTests::MakeStagesTable();
	RunProgressionTests::AddStage(*Stages, 1, 1500, 7, -25, 1);
	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->RegisterRunDataTables(Stages, RunProgressionTests::MakeBoostsTable());

	RunProgressionTests::SelectDice(*Manager, {1});
	TestTrue(TEXT("The losing round finishes"), Manager->FinishRound());
	TestEqual(TEXT("The configured loss reaches exactly zero"), Manager->GetMoney(), 0);
	TestTrue(TEXT("Zero money marks the run as game over"), Manager->IsGameOver());
	TestTrue(TEXT("Even the terminal loss enters the store phase"), Manager->IsStoreOpen());
	TestTrue(TEXT("The terminal store can be dismissed"), Manager->CloseStore());
	RunProgressionTests::SelectDice(*Manager, {1});
	TestEqual(TEXT("Dice input is ignored after game over"), Manager->GetCurrentScore(NAME_None), 0);
	TestFalse(TEXT("A new round cannot finish after game over"), Manager->FinishRound());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionStoreGenerationTest,
	"Daiso.RunProgression.Store.Generation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Проверяет размер, уникальность и строгое применение rarity-весов текущего раунда. */
bool FRunProgressionStoreGenerationTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = RunProgressionTests::MakeStagesTable();
	RunProgressionTests::AddStage(*Stages, 1, 1500, 7, -1, 4, 0.0f, 1.0f);
	UDataTable* Boosts = RunProgressionTests::MakeBoostsTable();
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		RunProgressionTests::AddBoost(*Boosts,
			FName(*FString::Printf(TEXT("R%02d"), Index)), EBoostRarity::Rare, 8, 2);
	}
	RunProgressionTests::AddBoost(*Boosts, TEXT("C01"), EBoostRarity::Common, 4, 3);

	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->RegisterRunDataTables(Stages, Boosts);
	Manager->SetStoreRandomSeedForTests(12345);
	RunProgressionTests::SelectDice(*Manager, {1});
	TestTrue(TEXT("The round opens a generated store"), Manager->FinishRound());

	const TArray<FBoostStoreOffer> Offers = Manager->GetStoreOffers();
	TestEqual(TEXT("Store uses the configured four slots"), Offers.Num(), 4);
	TSet<FName> UniqueIds;
	for (const FBoostStoreOffer& Offer : Offers)
	{
		TestEqual(TEXT("A one-hundred-percent Rare weight yields Rare offers"),
			Offer.Rarity, EBoostRarity::Rare);
		TestEqual(TEXT("Offer cost comes from DT_Boosts"), Offer.Cost, 8);
		UniqueIds.Add(Offer.BoostId);
	}
	TestEqual(TEXT("Generated cards do not repeat inside one store"), UniqueIds.Num(), 4);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionPurchaseTest,
	"Daiso.RunProgression.Store.Purchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Проверяет достаточность денег, списание цены, сохранение стака и одноразовость карточки. */
bool FRunProgressionPurchaseTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = RunProgressionTests::MakeStagesTable();
	RunProgressionTests::AddStage(*Stages, 1, 1500, 7, -1, 2);
	UDataTable* Boosts = RunProgressionTests::MakeBoostsTable();
	RunProgressionTests::AddBoost(*Boosts, TEXT("C01"), EBoostRarity::Common, 4, 2);
	RunProgressionTests::AddBoost(*Boosts, TEXT("C02"), EBoostRarity::Common, 30, 1);

	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->RegisterRunDataTables(Stages, Boosts);
	Manager->SetStoreRandomSeedForTests(7);
	RunProgressionTests::SelectDice(*Manager, {1});
	Manager->FinishRound();

	TestEqual(TEXT("The loss is applied before shopping"), Manager->GetMoney(), 24);
	TestFalse(TEXT("A boost cannot be bought without enough money"), Manager->PurchaseBoost(TEXT("C02")));
	TestTrue(TEXT("An affordable generated boost can be bought"), Manager->PurchaseBoost(TEXT("C01")));
	TestEqual(TEXT("Purchase subtracts the Data Table cost"), Manager->GetMoney(), 20);
	TestEqual(TEXT("Purchase persists one owned stack"), Manager->GetBoostStackCount(TEXT("C01")), 1);
	TestFalse(TEXT("The same store card cannot be bought twice"), Manager->PurchaseBoost(TEXT("C01")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionMaxStacksTest,
	"Daiso.RunProgression.Store.MaxStacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Покупает буст в двух магазинах и проверяет исключение строки после достижения MaxStacks. */
bool FRunProgressionMaxStacksTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = RunProgressionTests::MakeStagesTable();
	RunProgressionTests::AddStage(*Stages, 1, 1500, 7, -1, 1);
	UDataTable* Boosts = RunProgressionTests::MakeBoostsTable();
	RunProgressionTests::AddBoost(*Boosts, TEXT("C01"), EBoostRarity::Common, 1, 2,
		EBoostEffectTrigger::ScoreCalculated, EBoostEffectOperation::AddBasePerMatchingDie, 100.0f, 0.0f, 1);

	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->RegisterRunDataTables(Stages, Boosts);
	for (int32 PurchaseIndex = 0; PurchaseIndex < 2; ++PurchaseIndex)
	{
		RunProgressionTests::SelectDice(*Manager, {1});
		TestTrue(TEXT("A retry round finishes"), Manager->FinishRound());
		TestEqual(TEXT("The eligible boost is offered"), Manager->GetStoreOffers().Num(), 1);
		TestTrue(TEXT("The offered stack is purchased"), Manager->PurchaseBoost(TEXT("C01")));
		TestTrue(TEXT("The retry store closes"), Manager->CloseStore());
	}

	TestEqual(TEXT("The owned stack reaches MaxStacks"), Manager->GetBoostStackCount(TEXT("C01")), 2);
	RunProgressionTests::SelectDice(*Manager, {1});
	Manager->FinishRound();
	TestEqual(TEXT("A maxed boost is excluded from future generation"), Manager->GetStoreOffers().Num(), 0);
	TestFalse(TEXT("MaxStacks cannot be exceeded"), Manager->PurchaseBoost(TEXT("C01")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionBoostEffectsTest,
	"Daiso.RunProgression.Boosts.DataDrivenEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Проверяет тестовые операции Base, +Mult, условный XMult, Hot Dice и Farkle. */
bool FRunProgressionBoostEffectsTest::RunTest(const FString& Parameters)
{
	UDataTable* Stages = RunProgressionTests::MakeStagesTable();
	RunProgressionTests::AddStage(*Stages, 1, 1500, 7, -1, 0);
	UDataTable* Boosts = RunProgressionTests::MakeBoostsTable();
	RunProgressionTests::AddBoost(*Boosts, TEXT("C01"), EBoostRarity::Common, 4, 3,
		EBoostEffectTrigger::ScoreCalculated, EBoostEffectOperation::AddBasePerMatchingDie, 100.0f, 0.0f, 1);
	RunProgressionTests::AddBoost(*Boosts, TEXT("C05"), EBoostRarity::Common, 5, 2,
		EBoostEffectTrigger::ScoreCalculated, EBoostEffectOperation::AddMultiplierPerCombination, 1.0f);
	RunProgressionTests::AddBoost(*Boosts, TEXT("C06"), EBoostRarity::Common, 6, 2,
		EBoostEffectTrigger::HotDice, EBoostEffectOperation::AddMultiplier, 4.0f);
	RunProgressionTests::AddBoost(*Boosts, TEXT("C08"), EBoostRarity::Common, 6, 1,
		EBoostEffectTrigger::Farkle, EBoostEffectOperation::PreserveTurnScoreFraction, 0.5f);
	RunProgressionTests::AddBoost(*Boosts, TEXT("E01"), EBoostRarity::Epic, 13, 1,
		EBoostEffectTrigger::ScoreCalculated, EBoostEffectOperation::MultiplyScore, 2.0f, 10.0f);

	UGameManagerSubsystem* Manager = NewObject<UGameManagerSubsystem>();
	Manager->RegisterRunDataTables(Stages, Boosts);
	for (const FName BoostId : {FName(TEXT("C01")), FName(TEXT("C05")), FName(TEXT("C06")),
		FName(TEXT("C08")), FName(TEXT("E01"))})
	{
		TestTrue(FString::Printf(TEXT("%s test boost is granted"), *BoostId.ToString()),
			Manager->AddBoostStackForTests(BoostId));
	}

	FBoostEffectContext NormalContext;
	NormalContext.BaseScore = 100;
	NormalContext.BaseMultiplier = 8.0f;
	NormalContext.ScoredDiceValues = {1, 5};
	NormalContext.CombinationCount = 2;
	const FBoostEffectResult NormalResult = Manager->EvaluateBoostEffects(NormalContext);
	TestEqual(TEXT("Matching one adds 100 Base"), NormalResult.ModifiedBaseScore, 200);
	TestEqual(TEXT("Two combinations add two Mult"), NormalResult.AdditiveMultiplier, 10.0f);
	TestEqual(TEXT("Mult ten activates x2 XMult"), NormalResult.XMultiplier, 2.0f);
	TestEqual(TEXT("Base, Mult and XMult compose in the documented order"), NormalResult.FinalScore, 4000);

	FBoostEffectContext HotDiceContext;
	HotDiceContext.BaseScore = 100;
	HotDiceContext.BaseMultiplier = 1.0f;
	HotDiceContext.ScoredDiceValues = {5};
	HotDiceContext.CombinationCount = 1;
	HotDiceContext.bIsHotDice = true;
	const FBoostEffectResult HotDiceResult = Manager->EvaluateBoostEffects(HotDiceContext);
	TestEqual(TEXT("Hot Dice adds four Mult beside the general combination Mult"),
		HotDiceResult.AdditiveMultiplier, 6.0f);
	TestEqual(TEXT("Hot Dice result uses the additive multiplier"), HotDiceResult.FinalScore, 600);

	FBoostEffectContext FarkleContext;
	FarkleContext.bIsFarkle = true;
	FarkleContext.CurrentTurnScore = 500;
	const FBoostEffectResult FarkleResult = Manager->EvaluateBoostEffects(FarkleContext);
	TestEqual(TEXT("Farkle insurance preserves fifty percent of the turn"),
		FarkleResult.PreservedTurnScore, 250);
	TestEqual(TEXT("Farkle exposes the preserved value as its final result"), FarkleResult.FinalScore, 250);
	return !HasAnyErrors();
}

#endif
