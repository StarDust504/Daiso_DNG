// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystems/GameManagerSubsystem.h"

#include "Dice/CPP_Dice.h"
#include "Dice/DiceScoringLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"

namespace DiceSelection
{
	/** Преобразует массив значений в компактную строку для отладочного сообщения. */
	static FString ToString(const TArray<int32>& Values)
	{
		TArray<FString> Parts;
		for (const int32 Value : Values)
		{
			Parts.Add(FString::FromInt(Value));
		}
		return FString::Join(Parts, TEXT(","));
	}
}

/** Загружает штатные таблицы проекта и создаёт чистое состояние первого забега. */
void UGameManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DiceScoringRules = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	LevelGoalsTable = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/Data/DT_LevelGoals.DT_LevelGoals"));
	RunStagesTable = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/Data/DT_RunStages.DT_RunStages"));
	BoostsTable = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/Data/DT_Boosts.DT_Boosts"));
	StoreRandomStream.Initialize(FMath::Rand());
	ResetRun();
}

/** Добавляет выбранную грань только в игровом раунде и сразу пересчитывает комбинацию. */
FName UGameManagerSubsystem::AddComboToTempArray(const int32 NumberToAppend)
{
	if (bStoreOpen || bGameOver)
	{
		return BuildSelectedDiceKey();
	}

	TempScore.Add(NumberToAppend);
	TempScore.Sort();
	RefreshSelectionScore();
	return BuildSelectedDiceKey();
}

/** Удаляет одну совпадающую грань только в игровом раунде и пересчитывает оставшийся выбор. */
FName UGameManagerSubsystem::RemoveComboFromTempArray(const int32 NumberToRemove)
{
	if (bStoreOpen || bGameOver)
	{
		return BuildSelectedDiceKey();
	}

	if (!TempScore.IsEmpty() && TempScore.Contains(NumberToRemove))
	{
		TempScore.RemoveSingle(NumberToRemove);
		TempScore.Sort();
	}
	RefreshSelectionScore();
	return BuildSelectedDiceKey();
}

/** Возвращает текущий итог; старый параметр ключа оставлен ради совместимости Blueprint. */
int32 UGameManagerSubsystem::GetCurrentScore(FName)
{
	return LastSelectionScoreResult.TotalScore;
}

/** Возвращает копию отсортированных значений выбранных кубиков. */
TArray<int32> UGameManagerSubsystem::GetSelectedDiceValues() const
{
	return TempScore;
}

/** Возвращает последний результат scoring уже с применёнными эффектами купленных бустов. */
FDiceRollScoreResult UGameManagerSubsystem::GetSelectedDiceScore() const
{
	return LastSelectionScoreResult;
}

/** Сообщает, что все выбранные кубики входят в результативные комбинации. */
bool UGameManagerSubsystem::IsCurrentDiceSelectionValid() const
{
	return bIsCurrentSelectionValid;
}

/** Очищает выбор и публикует пустой результат подписчикам. */
void UGameManagerSubsystem::ClearDiceSelection()
{
	TempScore.Reset();
	RefreshSelectionScore();
}

/** Собирает совместимый снимок игрового счёта, цели, денег и текущей фазы забега. */
FLevelProgressState UGameManagerSubsystem::GetLevelProgress() const
{
	FLevelProgressState State;
	State.LevelNumber = CurrentLevelNumber;
	State.TargetScore = CurrentLevelTargetScore;
	State.CurrentScore = LastSelectionScoreResult.TotalScore;
	State.bCanFinishRound = !bStoreOpen
		&& !bGameOver
		&& bIsCurrentSelectionValid
		&& State.CurrentScore > 0;
	State.bLevelWon = bLevelWon;
	State.Money = Money;
	State.bInStore = bStoreOpen;
	State.bGameOver = bGameOver;
	State.bLastRoundWon = bLastRoundWon;
	return State;
}

/** Собирает полный снимок run-state для UMG и внешней Blueprint-логики. */
FRunProgressState UGameManagerSubsystem::GetRunProgress() const
{
	FRunProgressState State;
	State.RoundNumber = CurrentLevelNumber;
	State.RequiredInvoice = CurrentLevelTargetScore;
	State.Money = Money;
	State.bStoreOpen = bStoreOpen;
	State.bGameOver = bGameOver;
	State.bLastRoundWon = bLastRoundWon;
	State.OwnedBoosts = OwnedBoosts;
	State.StoreOffers = CurrentStoreOffers;
	return State;
}

/** Возвращает число стаков буста из состояния забега без обращения к Data Table. */
int32 UGameManagerSubsystem::GetBoostStackCount(const FName BoostId) const
{
	for (const FOwnedBoostStack& OwnedBoost : OwnedBoosts)
	{
		if (OwnedBoost.BoostId == BoostId)
		{
			return OwnedBoost.StackCount;
		}
	}
	return 0;
}

/**
 * Разрешает исход раунда, применяет экономику строки и открывает магазин той же строки.
 * Следующая цель намеренно загружается только после CloseStore, чтобы UI не смешивал фазы.
 */
bool UGameManagerSubsystem::FinishRound()
{
	if (!IsValid(RunStagesTable) && !IsValid(LevelGoalsTable))
	{
		LoadCurrentRunStage();
	}

	FLevelProgressState FinishedRound = GetLevelProgress();
	if (!FinishedRound.bCanFinishRound)
	{
		return false;
	}

	const FRunStageRow CompletedStage = CurrentRunStage;
	const bool bReachedGoal = FinishedRound.CurrentScore >= CompletedStage.RequiredInvoice;
	FinishedRound.bLevelWon = bReachedGoal;
	FinishedRound.bLastRoundWon = bReachedGoal;

	bLastRoundWon = bReachedGoal;
	bLevelWon = bReachedGoal;
	PendingLevelNumber = CurrentLevelNumber;
	if (bReachedGoal)
	{
		FRunStageRow NextStage;
		if (TryGetRunStage(CurrentLevelNumber + 1, NextStage))
		{
			PendingLevelNumber = CurrentLevelNumber + 1;
		}
	}

	ChangeMoney(bReachedGoal ? CompletedStage.Win : CompletedStage.Lose);
	ResetDiceAfterFinishedRound();
	ResetRoundScore();

	bStoreOpen = true;
	GenerateStoreOffers(CompletedStage);
	bGameOver = Money <= 0;
	RefreshStoreOfferAvailability();

	OnDiceSelectionChanged.Broadcast(LastSelectionScoreResult);
	PublishLevelProgress();
	if (bReachedGoal)
	{
		OnLevelWon.Broadcast(FinishedRound);
	}
	OnStoreOffersChanged.Broadcast(CurrentStoreOffers);
	OnStoreOpened.Broadcast(GetRunProgress());
	if (bGameOver)
	{
		OnGameOver.Broadcast(GetRunProgress());
	}
	return true;
}

/** Проверяет предложение, списывает цену и сохраняет новый стак в едином состоянии забега. */
bool UGameManagerSubsystem::PurchaseBoost(const FName BoostId)
{
	if (!bStoreOpen || bGameOver || BoostId.IsNone())
	{
		return false;
	}

	FBoostStoreOffer* Offer = CurrentStoreOffers.FindByPredicate(
		[BoostId](const FBoostStoreOffer& Candidate)
		{
			return Candidate.BoostId == BoostId && !Candidate.bPurchased;
		});
	const FBoostRow* Row = FindBoostRow(BoostId);
	if (!Offer || !Row || !Offer->bCanPurchase || Money < Row->Cost
		|| GetBoostStackCount(BoostId) >= Row->MaxStacks)
	{
		return false;
	}

	int32 NewStackCount = 0;
	if (!AddBoostStack(BoostId, *Row, NewStackCount))
	{
		return false;
	}

	Offer->bPurchased = true;
	ChangeMoney(-Row->Cost);
	if (Money <= 0)
	{
		bGameOver = true;
	}
	RefreshStoreOfferAvailability();

	const FBoostStoreOffer PurchasedOffer = *Offer;
	OnStoreOffersChanged.Broadcast(CurrentStoreOffers);
	OnBoostPurchased.Broadcast(PurchasedOffer, NewStackCount);
	PublishLevelProgress();
	if (bGameOver)
	{
		OnGameOver.Broadcast(GetRunProgress());
	}
	return true;
}

/** Закрывает магазин и только при живом забеге переводит подсистему к отложенной цели. */
bool UGameManagerSubsystem::CloseStore()
{
	if (!bStoreOpen)
	{
		return false;
	}

	bStoreOpen = false;
	CurrentStoreOffers.Reset();
	OnStoreOffersChanged.Broadcast(CurrentStoreOffers);

	if (!bGameOver)
	{
		const int32 PreviousLevelNumber = CurrentLevelNumber;
		const FRunStageRow PreviousStage = CurrentRunStage;
		CurrentLevelNumber = PendingLevelNumber;
		if (!LoadCurrentRunStage())
		{
			CurrentLevelNumber = PreviousLevelNumber;
			CurrentRunStage = PreviousStage;
			CurrentLevelTargetScore = PreviousStage.RequiredInvoice;
		}
		bLevelWon = false;
		ResetRoundScore();
	}

	PublishLevelProgress();
	OnStoreClosed.Broadcast(GetRunProgress());
	return true;
}

/** Сбрасывает всё состояние забега и повторно загружает первую строку DT_RunStages. */
void UGameManagerSubsystem::ResetRun()
{
	const int32 PreviousMoney = Money;
	ResetDiceAfterFinishedRound();
	ResetRoundScore();
	OwnedBoosts.Reset();
	CurrentStoreOffers.Reset();
	CurrentLevelNumber = 1;
	PendingLevelNumber = 1;
	Money = 25;
	bLevelWon = false;
	bLastRoundWon = false;
	bStoreOpen = false;
	bGameOver = false;
	LoadCurrentRunStage();

	if (PreviousMoney != Money)
	{
		OnMoneyChanged.Broadcast(Money, Money - PreviousMoney);
	}
	OnStoreOffersChanged.Broadcast(CurrentStoreOffers);
	OnDiceSelectionChanged.Broadcast(LastSelectionScoreResult);
	PublishLevelProgress();
}

/**
 * Выполняет поддержанные операции независимо от ID буста: поведение задаётся колонками
 * EffectTrigger/EffectOperation/Magnitude/Threshold/FaceValue в DT_Boosts.
 */
FBoostEffectResult UGameManagerSubsystem::EvaluateBoostEffects(const FBoostEffectContext& Context) const
{
	FBoostEffectResult Result;
	Result.ModifiedBaseScore = FMath::Max(0, Context.BaseScore);
	Result.AdditiveMultiplier = FMath::Max(0.0f, Context.BaseMultiplier);
	Result.XMultiplier = 1.0f;

	struct FActiveEffect
	{
		const FBoostRow* Row = nullptr;
		int32 Stacks = 0;
	};
	TArray<FActiveEffect> ActiveEffects;
	for (const FOwnedBoostStack& OwnedBoost : OwnedBoosts)
	{
		const FBoostRow* Row = FindBoostRow(OwnedBoost.BoostId);
		if (Row && OwnedBoost.StackCount > 0 && DoesEffectTriggerMatch(*Row, Context))
		{
			ActiveEffects.Add({Row, OwnedBoost.StackCount});
		}
	}

	// Первый проход меняет только Base, поэтому результат не зависит от порядка покупок.
	for (const FActiveEffect& Effect : ActiveEffects)
	{
		const FBoostRow& Row = *Effect.Row;
		if (Row.EffectOperation == EBoostEffectOperation::AddBasePerMatchingDie)
		{
			int32 MatchingDice = 0;
			for (const int32 Face : Context.ScoredDiceValues)
			{
				MatchingDice += Row.EffectFaceValue == 0 || Face == Row.EffectFaceValue ? 1 : 0;
			}
			Result.ModifiedBaseScore += FMath::RoundToInt(
				Row.EffectMagnitude * static_cast<float>(MatchingDice * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddBasePerCombination)
		{
			Result.ModifiedBaseScore += FMath::RoundToInt(
				Row.EffectMagnitude * static_cast<float>(Context.CombinationCount * Effect.Stacks));
		}
	}

	// Второй проход накапливает обычный +Mult поверх базового множителя контекста.
	for (const FActiveEffect& Effect : ActiveEffects)
	{
		const FBoostRow& Row = *Effect.Row;
		if (Row.EffectOperation == EBoostEffectOperation::AddMultiplier)
		{
			Result.AdditiveMultiplier += Row.EffectMagnitude * static_cast<float>(Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerCombination)
		{
			Result.AdditiveMultiplier += Row.EffectMagnitude
				* static_cast<float>(Context.CombinationCount * Effect.Stacks);
		}
	}

	// Третий проход видит уже итоговый +Mult и применяет условные XMult мультипликативно.
	for (const FActiveEffect& Effect : ActiveEffects)
	{
		const FBoostRow& Row = *Effect.Row;
		if (Row.EffectOperation == EBoostEffectOperation::MultiplyScore
			&& (Row.EffectThreshold <= 0.0f || Result.AdditiveMultiplier >= Row.EffectThreshold))
		{
			Result.XMultiplier *= FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude), Effect.Stacks);
		}
	}

	// Специальный проход Farkle возвращает сохранённую часть хода отдельно от обычного score.
	for (const FActiveEffect& Effect : ActiveEffects)
	{
		const FBoostRow& Row = *Effect.Row;
		if (Row.EffectOperation == EBoostEffectOperation::PreserveTurnScoreFraction)
		{
			const float PreservedFraction = FMath::Clamp(
				Row.EffectMagnitude * static_cast<float>(Effect.Stacks), 0.0f, 1.0f);
			Result.PreservedTurnScore = FMath::Max(
				Result.PreservedTurnScore,
				FMath::RoundToInt(static_cast<float>(Context.CurrentTurnScore) * PreservedFraction));
		}
	}

	const double CalculatedScore = static_cast<double>(Result.ModifiedBaseScore)
		* static_cast<double>(Result.AdditiveMultiplier)
		* static_cast<double>(Result.XMultiplier);
	Result.FinalScore = Context.bIsFarkle
		? Result.PreservedTurnScore
		: static_cast<int32>(FMath::Clamp(
			FMath::RoundToDouble(CalculatedScore), 0.0, static_cast<double>(MAX_int32)));
	return Result;
}

/** Переключает отладочную/Blueprint-цель, сохраняя деньги и купленные бусты текущего забега. */
bool UGameManagerSubsystem::SetCurrentLevelNumber(const int32 NewLevelNumber)
{
	if (NewLevelNumber < 1 || bGameOver)
	{
		return false;
	}

	const int32 PreviousLevelNumber = CurrentLevelNumber;
	const FRunStageRow PreviousStage = CurrentRunStage;
	CurrentLevelNumber = NewLevelNumber;
	if (!LoadCurrentRunStage())
	{
		CurrentLevelNumber = PreviousLevelNumber;
		CurrentRunStage = PreviousStage;
		CurrentLevelTargetScore = PreviousStage.RequiredInvoice;
		return false;
	}

	PendingLevelNumber = CurrentLevelNumber;
	bStoreOpen = false;
	bLevelWon = false;
	bLastRoundWon = false;
	CurrentStoreOffers.Reset();
	ResetDiceAfterFinishedRound();
	ResetRoundScore();
	OnStoreOffersChanged.Broadcast(CurrentStoreOffers);
	OnDiceSelectionChanged.Broadcast(LastSelectionScoreResult);
	PublishLevelProgress();
	return true;
}

/** Подменяет обе progression-таблицы одним согласованным вызовом и начинает чистый забег. */
void UGameManagerSubsystem::RegisterRunDataTables(UDataTable* RunStagesDT, UDataTable* BoostsDT)
{
	RunStagesTable = RunStagesDT;
	BoostsTable = BoostsDT;
	ResetRun();
}

/** Собирает совместимый FName-ключ из отсортированных значений выбранных кубиков. */
FName UGameManagerSubsystem::BuildSelectedDiceKey() const
{
	FString Key;
	for (const int32 Value : TempScore)
	{
		Key.AppendInt(Value);
	}
	return Key.IsEmpty() ? NAME_None : FName(*Key);
}

/** Пересчитывает старый scorer и накладывает поддержанные бусты, не меняя найденные комбинации. */
void UGameManagerSubsystem::RefreshSelectionScore()
{
	if (!IsValid(DiceScoringRules))
	{
		DiceScoringRules = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	}

	const FDiceRollScoreResult RawResult = TempScore.IsEmpty()
		? FDiceRollScoreResult()
		: UDiceScoringLibrary::CalculateSelectedDiceScore(TempScore, DiceScoringRules);
	LastSelectionScoreResult = RawResult;
	if (RawResult.bIsValid)
	{
		FBoostEffectContext Context;
		Context.BaseScore = RawResult.TotalScore;
		Context.BaseMultiplier = 1.0f;
		Context.CombinationCount = RawResult.Combinations.Num();
		Context.CurrentTurnScore = RawResult.TotalScore;
		Context.bIsFarkle = RawResult.bIsBust;
		Context.bIsHotDice = TempScore.Num() == 6 && RawResult.bAllDiceScored;
		for (const FDiceScoringCombination& Combination : RawResult.Combinations)
		{
			Context.ScoredDiceValues.Append(Combination.DiceValues);
		}
		LastSelectionScoreResult.TotalScore = EvaluateBoostEffects(Context).FinalScore;
	}

	bIsCurrentSelectionValid = LastSelectionScoreResult.bIsValid
		&& LastSelectionScoreResult.bAllDiceScored;
	OnDiceSelectionChanged.Broadcast(LastSelectionScoreResult);
	PublishLevelProgress();

	const FString SelectedText = DiceSelection::ToString(TempScore);
	const FString UnscoredText = DiceSelection::ToString(LastSelectionScoreResult.UnscoredDiceValues);
	const FString Message = FString::Printf(
		TEXT("Selected dice: [%s] | score: %d | valid: %s%s"),
		*SelectedText,
		LastSelectionScoreResult.TotalScore,
		bIsCurrentSelectionValid ? TEXT("YES") : TEXT("NO"),
		UnscoredText.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | unscored: [%s]"), *UnscoredText));

	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 3.0f, bIsCurrentSelectionValid ? FColor::Green : FColor::Yellow, Message);
	}
}

/** Лениво загружает DT_RunStages и фиксирует проверенную строку как активную цель. */
bool UGameManagerSubsystem::LoadCurrentRunStage()
{
	if (!IsValid(RunStagesTable))
	{
		RunStagesTable = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Data/DT_RunStages.DT_RunStages"));
	}
	if (!IsValid(LevelGoalsTable))
	{
		LevelGoalsTable = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Data/DT_LevelGoals.DT_LevelGoals"));
	}

	FRunStageRow LoadedStage;
	if (!TryGetRunStage(CurrentLevelNumber, LoadedStage))
	{
		UE_LOG(LogTemp, Warning, TEXT("No valid progression row for round %d."), CurrentLevelNumber);
		return false;
	}

	CurrentRunStage = LoadedStage;
	CurrentLevelTargetScore = LoadedStage.RequiredInvoice;
	return true;
}

/** Ищет строку сначала по стабильному имени Round_XX, затем по полю Round. */
bool UGameManagerSubsystem::TryGetRunStage(const int32 RoundNumber, FRunStageRow& OutStage) const
{
	if (IsValid(RunStagesTable) && IsValid(RunStagesTable->GetRowStruct())
		&& RunStagesTable->GetRowStruct()->IsChildOf(FRunStageRow::StaticStruct()))
	{
		const FName ExpectedRowName(*FString::Printf(TEXT("Round_%02d"), RoundNumber));
		const FRunStageRow* Stage = RunStagesTable->FindRow<FRunStageRow>(
			ExpectedRowName, TEXT("GameManagerSubsystem::TryGetRunStage"), false);
		if (!Stage)
		{
			for (const FName RowName : RunStagesTable->GetRowNames())
			{
				const FRunStageRow* Candidate = RunStagesTable->FindRow<FRunStageRow>(
					RowName, TEXT("GameManagerSubsystem::TryGetRunStage"), false);
				if (Candidate && Candidate->Round == RoundNumber)
				{
					Stage = Candidate;
					break;
				}
			}
		}

		if (Stage && Stage->Round == RoundNumber && Stage->RequiredInvoice > 0 && Stage->Store >= 0
			&& Stage->Common >= 0.0f && Stage->Rare >= 0.0f
			&& Stage->Epic >= 0.0f && Stage->Legendary >= 0.0f)
		{
			OutStage = *Stage;
			return true;
		}
		return false;
	}

	// Legacy fallback сохраняет старые цели, если новые assets ещё не импортированы.
	if (IsValid(LevelGoalsTable) && IsValid(LevelGoalsTable->GetRowStruct())
		&& LevelGoalsTable->GetRowStruct()->IsChildOf(FLevelGoalRow::StaticStruct()))
	{
		const FName RowName(*FString::Printf(TEXT("Level_%02d"), RoundNumber));
		const FLevelGoalRow* Goal = LevelGoalsTable->FindRow<FLevelGoalRow>(
			RowName, TEXT("GameManagerSubsystem::TryGetRunStageLegacy"), false);
		if (Goal && Goal->LevelNumber == RoundNumber && Goal->TargetScore > 0)
		{
			OutStage = FRunStageRow();
			OutStage.Round = RoundNumber;
			OutStage.RequiredInvoice = Goal->TargetScore;
			return true;
		}
	}
	return false;
}

/** Возвращает строго типизированную строку буста либо nullptr при ошибке конфигурации. */
const FBoostRow* UGameManagerSubsystem::FindBoostRow(const FName BoostId) const
{
	if (BoostId.IsNone() || !IsValid(BoostsTable) || !IsValid(BoostsTable->GetRowStruct())
		|| !BoostsTable->GetRowStruct()->IsChildOf(FBoostRow::StaticStruct()))
	{
		return nullptr;
	}
	return BoostsTable->FindRow<FBoostRow>(
		BoostId, TEXT("GameManagerSubsystem::FindBoostRow"), false);
}

/** Рассылает подписчикам актуальный совместимый снимок прогрессии. */
void UGameManagerSubsystem::PublishLevelProgress()
{
	OnLevelProgressChanged.Broadcast(GetLevelProgress());
}

/** Безопасно меняет int32-баланс и сообщает UI фактически применённую разницу. */
void UGameManagerSubsystem::ChangeMoney(const int32 Delta)
{
	const int32 PreviousMoney = Money;
	Money = static_cast<int32>(FMath::Clamp(
		static_cast<int64>(Money) + static_cast<int64>(Delta),
		static_cast<int64>(MIN_int32), static_cast<int64>(MAX_int32)));
	OnMoneyChanged.Broadcast(Money, Money - PreviousMoney);
}

/** Генерирует неповторяющиеся предложения, исключая уже достигшие MaxStacks бусты. */
void UGameManagerSubsystem::GenerateStoreOffers(const FRunStageRow& CompletedStage)
{
	CurrentStoreOffers.Reset();
	if (CompletedStage.Store <= 0 || !IsValid(BoostsTable)
		|| !IsValid(BoostsTable->GetRowStruct())
		|| !BoostsTable->GetRowStruct()->IsChildOf(FBoostRow::StaticStruct()))
	{
		return;
	}

	TArray<FName> RowNames = BoostsTable->GetRowNames();
	RowNames.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	TSet<FName> SelectedIds;
	for (int32 SlotIndex = 0; SlotIndex < CompletedStage.Store; ++SlotIndex)
	{
		const EBoostRarity RolledRarity = RollStoreRarity(CompletedStage);
		TArray<FName> MatchingCandidates;
		TArray<FName> AnyRarityCandidates;
		for (const FName RowName : RowNames)
		{
			const FBoostRow* Row = FindBoostRow(RowName);
			if (!Row || SelectedIds.Contains(RowName) || Row->MaxStacks <= 0
				|| GetBoostStackCount(RowName) >= Row->MaxStacks)
			{
				continue;
			}

			AnyRarityCandidates.Add(RowName);
			if (Row->Rarity == RolledRarity)
			{
				MatchingCandidates.Add(RowName);
			}
		}

		const TArray<FName>& Candidates = MatchingCandidates.IsEmpty()
			? AnyRarityCandidates
			: MatchingCandidates;
		if (Candidates.IsEmpty())
		{
			break;
		}

		const FName ChosenId = Candidates[StoreRandomStream.RandRange(0, Candidates.Num() - 1)];
		if (const FBoostRow* ChosenRow = FindBoostRow(ChosenId))
		{
			SelectedIds.Add(ChosenId);
			CurrentStoreOffers.Add(MakeStoreOffer(ChosenId, *ChosenRow));
		}
	}
	RefreshStoreOfferAvailability();
}

/** Выполняет один weighted roll; веса могут быть заданы долями или процентами одной шкалы. */
EBoostRarity UGameManagerSubsystem::RollStoreRarity(const FRunStageRow& Stage)
{
	const float TotalWeight = Stage.Common + Stage.Rare + Stage.Epic + Stage.Legendary;
	if (TotalWeight <= UE_SMALL_NUMBER)
	{
		return EBoostRarity::Common;
	}

	const float Roll = StoreRandomStream.FRandRange(0.0f, TotalWeight);
	if (Roll < Stage.Common)
	{
		return EBoostRarity::Common;
	}
	if (Roll < Stage.Common + Stage.Rare)
	{
		return EBoostRarity::Rare;
	}
	if (Roll < Stage.Common + Stage.Rare + Stage.Epic)
	{
		return EBoostRarity::Epic;
	}
	return EBoostRarity::Legendary;
}

/** Копирует витринные поля Data Table и добавляет динамические данные стаков/доступности. */
FBoostStoreOffer UGameManagerSubsystem::MakeStoreOffer(const FName BoostId, const FBoostRow& Row) const
{
	FBoostStoreOffer Offer;
	Offer.BoostId = BoostId;
	Offer.DisplayName = Row.DisplayName;
	Offer.Rarity = Row.Rarity;
	Offer.Cost = Row.Cost;
	Offer.EffectDescription = Row.EffectDescription;
	Offer.CurrentStacks = GetBoostStackCount(BoostId);
	Offer.MaxStacks = Row.MaxStacks;
	Offer.bCanPurchase = !bGameOver && Money >= Row.Cost && Offer.CurrentStacks < Offer.MaxStacks;
	return Offer;
}

/** Обновляет каждую карточку после изменения денег, Game Over или числа стаков. */
void UGameManagerSubsystem::RefreshStoreOfferAvailability()
{
	for (FBoostStoreOffer& Offer : CurrentStoreOffers)
	{
		const FBoostRow* Row = FindBoostRow(Offer.BoostId);
		Offer.CurrentStacks = GetBoostStackCount(Offer.BoostId);
		Offer.bCanPurchase = Row
			&& !Offer.bPurchased
			&& !bGameOver
			&& Money >= Row->Cost
			&& Offer.CurrentStacks < Row->MaxStacks;
	}
}

/** Добавляет ровно один стак, не позволяя состоянию забега превысить ограничение строки. */
bool UGameManagerSubsystem::AddBoostStack(
	const FName BoostId, const FBoostRow& Row, int32& OutNewStackCount)
{
	OutNewStackCount = 0;
	if (BoostId.IsNone() || Row.MaxStacks <= 0)
	{
		return false;
	}

	for (FOwnedBoostStack& OwnedBoost : OwnedBoosts)
	{
		if (OwnedBoost.BoostId == BoostId)
		{
			if (OwnedBoost.StackCount >= Row.MaxStacks)
			{
				return false;
			}
			OutNewStackCount = ++OwnedBoost.StackCount;
			return true;
		}
	}

	FOwnedBoostStack& NewOwnedBoost = OwnedBoosts.AddDefaulted_GetRef();
	NewOwnedBoost.BoostId = BoostId;
	NewOwnedBoost.StackCount = 1;
	OutNewStackCount = 1;
	return true;
}

/** Сопоставляет строку эффекта с обычным scoring, Farkle либо Hot Dice событием контекста. */
bool UGameManagerSubsystem::DoesEffectTriggerMatch(
	const FBoostRow& Row, const FBoostEffectContext& Context) const
{
	switch (Row.EffectTrigger)
	{
	case EBoostEffectTrigger::ScoreCalculated:
		return !Context.bIsFarkle && Context.BaseScore > 0;
	case EBoostEffectTrigger::Farkle:
		return Context.bIsFarkle;
	case EBoostEffectTrigger::HotDice:
		return Context.bIsHotDice;
	default:
		return false;
	}
}

/** Возвращает выбранные кубики в обычное состояние и очищает их внутренний реестр. */
void UGameManagerSubsystem::ResetDiceAfterFinishedRound()
{
	for (ACPP_Dice* Dice : RegisteredDice)
	{
		if (!IsValid(Dice))
		{
			continue;
		}

		Dice->SetCanRollDice(true);
		Dice->SetIsActive(false);
		if (Dice->bIsHidden)
		{
			Dice->ShowDiceEffect(false);
			Dice->bIsHidden = false;
		}
	}
	RegisteredDice.Reset();
}

/** Обнуляет выбранные грани и scoring-флаги, сохраняя progression между фазами. */
void UGameManagerSubsystem::ResetRoundScore()
{
	TempScore.Reset();
	LastSelectionScoreResult = FDiceRollScoreResult();
	bIsCurrentSelectionValid = false;
}

/** Добавляет валидный кубик в реестр без дубликатов. */
void UGameManagerSubsystem::RegisterDice(ACPP_Dice* DiceToRegister)
{
	if (!IsValid(DiceToRegister) || RegisteredDice.Contains(DiceToRegister))
	{
		return;
	}

	DiceToRegister->SetCanRollDice(false);
	RegisteredDice.Add(DiceToRegister);
}

/** Удаляет кубик из реестра и снова разрешает ему бросок. */
void UGameManagerSubsystem::UnregisterDice(ACPP_Dice* DiceToUnregister)
{
	if (!RegisteredDice.Contains(DiceToUnregister))
	{
		return;
	}

	DiceToUnregister->SetCanRollDice(true);
	RegisteredDice.Remove(DiceToUnregister);
}

/** Проверяет наличие кубика в реестре подсистемы. */
bool UGameManagerSubsystem::CheckIsDiceRegistered(ACPP_Dice* DiceToCheck) const
{
	return RegisteredDice.Contains(DiceToCheck);
}

/** Сохраняет существующую логику скрытия неполного набора и освобождения полного набора костей. */
void UGameManagerSubsystem::DestroyRegisteredDice()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 5.f, FColor::Magenta, FString::FromInt(RegisteredDice.Num()));
	}
	if (RegisteredDice.Num() < 6)
	{
		for (ACPP_Dice* Dice : RegisteredDice)
		{
			if (IsValid(Dice) && !Dice->bIsHidden)
			{
				Dice->ShowDiceEffect(true);
				Dice->bIsHidden = true;
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < 6; ++Index)
		{
			ACPP_Dice* Dice = RegisteredDice[Index];
			if (!IsValid(Dice))
			{
				continue;
			}
			if (Dice->bIsHidden)
			{
				Dice->ShowDiceEffect(false);
				Dice->bIsHidden = false;
			}
			Dice->SetCanRollDice(true);
			Dice->SetIsActive(false);
		}
		RegisteredDice.Empty();
	}
}

/** Сохраняет ссылку на старую таблицу очков для существующей Blueprint-логики. */
void UGameManagerSubsystem::RegisterScoreDataTable(UDataTable* ScoreDT)
{
	ScoreDataTable = ScoreDT;
}

#if WITH_DEV_AUTOMATION_TESTS
/** Добавляет тестовый стак через production-проверку MaxStacks, не создавая магазинную экономику. */
bool UGameManagerSubsystem::AddBoostStackForTests(const FName BoostId)
{
	const FBoostRow* Row = FindBoostRow(BoostId);
	int32 NewStackCount = 0;
	return Row && AddBoostStack(BoostId, *Row, NewStackCount);
}

/** Переинициализирует FRandomStream заданным seed для стабильного результата Automation Tests. */
void UGameManagerSubsystem::SetStoreRandomSeedForTests(const int32 Seed)
{
	StoreRandomStream.Initialize(Seed);
}
#endif
