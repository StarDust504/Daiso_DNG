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

/** Заменяет текущий выбор одним проверенным набором граней и публикует только готовый результат. */
bool UGameManagerSubsystem::SetDiceRollSelection(const TArray<int32>& DiceValues)
{
	if (bStoreOpen || bGameOver || DiceValues.Num() > 6)
	{
		return false;
	}
	for (const int32 Value : DiceValues)
	{
		if (Value < 1 || Value > 6)
		{
			return false;
		}
	}

	TempScore = DiceValues;
	TempScore.Sort();
	RefreshSelectionScore();
	return true;
}

/** Снимок состояния до физического броска отделяет реальные игровые события от кликов по UI. */
void UGameManagerSubsystem::NotifyDiceRollStarted()
{
	bCurrentRollIsReroll = bHasResolvedRollThisRound;
	bHotDiceBeforeCurrentRoll = TempScore.Num() == 6 && LastSelectionScoreResult.bAllDiceScored;
	XMultiplierActivationsBeforeCurrentRoll = RoundXMultiplierActivations;
	CurrentRollHotCycleXMultiplier = 1.0f;
	CurrentRollPurchaseXMultiplier = 1.0f;
}

/** Продвигает все stateful-эффекты ровно один раз после завершения физического броска. */
void UGameManagerSubsystem::NotifyDiceRollResolved(const TArray<int32>& RolledDiceValues)
{
	if (bStoreOpen || bGameOver || RolledDiceValues.IsEmpty() || RolledDiceValues.Num() > 6)
	{
		return;
	}
	for (const int32 Face : RolledDiceValues)
	{
		if (Face < 1 || Face > 6)
		{
			return;
		}
	}
	if (!IsValid(DiceScoringRules))
	{
		DiceScoringRules = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	}

	const FDiceRollScoreResult RollResult = UDiceScoringLibrary::CalculateSelectedDiceScore(
		RolledDiceValues, DiceScoringRules);
	const bool bSuccessfulRoll = RollResult.bIsValid && RollResult.TotalScore > 0;

	// E05 consumes an already armed Hot Dice cycle before this roll can arm the next one.
	if (PendingHotDiceFullCycles > 0 && RolledDiceValues.Num() == 6)
	{
		for (const FOwnedBoostStack& Owned : OwnedBoosts)
		{
			const FBoostRow* Row = FindBoostRow(Owned.BoostId);
			if (Row && Row->EffectOperation == EBoostEffectOperation::MultiplyNextFullCycleAfterHotDice)
			{
				CurrentRollHotCycleXMultiplier = FMath::Pow(
					FMath::Max(0.0f, Row->EffectMagnitude), PendingHotDiceFullCycles * Owned.StackCount);
				break;
			}
		}
		PendingHotDiceFullCycles = 0;
	}

	if (bSuccessfulRoll && ActiveAcceleratorPurchases > 0)
	{
		for (const FOwnedBoostStack& Owned : OwnedBoosts)
		{
			const FBoostRow* Row = FindBoostRow(Owned.BoostId);
			if (Row && Row->EffectOperation == EBoostEffectOperation::MultiplyFirstCombinationAfterPurchase)
			{
				CurrentRollPurchaseXMultiplier = FMath::Pow(
					FMath::Max(0.0f, Row->EffectMagnitude), ActiveAcceleratorPurchases);
				break;
			}
		}
		ActiveAcceleratorPurchases = 0;
	}

	if (bSuccessfulRoll)
	{
		++SuccessfulRollsThisTurn;
		++ConsecutiveSuccessfulRolls;
		if (bCurrentRollIsReroll)
		{
			++ConsecutiveSuccessfulRerolls;
		}

		for (const FDiceScoringCombination& Combination : RollResult.Combinations)
		{
			++CombinationCountThisRound;
			CombinationRulesThisRound.Add(Combination.RuleRowName);
			CombinationTypesThisTurn.Add(static_cast<uint8>(Combination.CombinationType));
			LastScoringCombinationScore = Combination.Score;
			for (const int32 Face : Combination.DiceValues)
			{
				ScoredOnesThisRound += Face == 1 ? 1 : 0;
				ScoredFivesThisRound += Face == 5 ? 1 : 0;
			}

			if (Combination.CombinationType == EDiceScoringCombinationType::SameFace)
			{
				++SetCountThisRound;
				int32 NewRetriggers = 0;
				for (const FOwnedBoostStack& Owned : OwnedBoosts)
				{
					const FBoostRow* Row = FindBoostRow(Owned.BoostId);
					if (!Row || Row->EffectOperation != EBoostEffectOperation::RetriggerEveryNthSet)
					{
						continue;
					}
					const int32 Frequency = FMath::Max(1, FMath::RoundToInt(Row->EffectThreshold));
					if (SetCountThisRound % Frequency == 0)
					{
						NewRetriggers += FMath::Max(0,
							FMath::RoundToInt(Row->EffectMagnitude) * Owned.StackCount);
					}
				}
				if (NewRetriggers > 0)
				{
					RetriggeredBaseScoreThisTurn += Combination.Score * NewRetriggers;
					RetriggerCountThisTurn += NewRetriggers;
					if (!bRecursiveRetriggerUsedThisTurn
						&& GetOwnedStacksForOperation(EBoostEffectOperation::RecursiveFirstRetrigger) > 0)
					{
						RetriggeredBaseScoreThisTurn += Combination.Score;
						++RetriggerCountThisTurn;
						bRecursiveRetriggerUsedThisTurn = true;
					}
				}
			}
			else if (Combination.CombinationType == EDiceScoringCombinationType::Straight)
			{
				for (const FOwnedBoostStack& Owned : OwnedBoosts)
				{
					const FBoostRow* Row = FindBoostRow(Owned.BoostId);
					if (Row && Row->EffectOperation == EBoostEffectOperation::AddMultiplierPerStraight)
					{
						const float Bonus = Combination.DiceValues.Num() == 6
							? Row->EffectSecondaryMagnitude : Row->EffectMagnitude;
						StraightMultiplierThisTurn += FMath::RoundToInt(Bonus * Owned.StackCount);
					}
				}
			}
		}
	}
	else
	{
		ConsecutiveSuccessfulRolls = 0;
		ConsecutiveSuccessfulRerolls = 0;
		SuccessfulRollsThisTurn = 0;
	}

	const bool bHotDiceNow = TempScore.Num() == 6 && LastSelectionScoreResult.bAllDiceScored;
	if (bHotDiceNow && !bHotDiceBeforeCurrentRoll)
	{
		++HotDiceCountThisRound;
		if (GetOwnedStacksForOperation(EBoostEffectOperation::MultiplyNextFullCycleAfterHotDice) > 0)
		{
			++PendingHotDiceFullCycles;
		}

		const int32 EternalEngineStacks = GetOwnedStacksForOperation(
			EBoostEffectOperation::RetriggerLastCombinationOnHotDice);
		if (EternalEngineStacks > 0 && LastScoringCombinationScore > 0)
		{
			const int32 Repeats = HotDiceCountThisRound * EternalEngineStacks;
			RetriggeredBaseScoreThisTurn += LastScoringCombinationScore * Repeats;
			RetriggerCountThisTurn += Repeats;
			if (!bRecursiveRetriggerUsedThisTurn
				&& GetOwnedStacksForOperation(EBoostEffectOperation::RecursiveFirstRetrigger) > 0)
			{
				RetriggeredBaseScoreThisTurn += LastScoringCombinationScore;
				++RetriggerCountThisTurn;
				bRecursiveRetriggerUsedThisTurn = true;
			}
		}
	}

	RefreshSelectionScore();
	RoundXMultiplierActivations += LastBoostEffectResult.ActivatedXMultiplierCount;
	bHasResolvedRollThisRound = true;
	bCurrentRollIsReroll = false;
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

	GrantScoreBoostMoney(FinishedRound.CurrentScore);
	if (bReachedGoal && GetOwnedStacksForOperation(EBoostEffectOperation::MultiplyPerWin) > 0)
	{
		++RunWinBoostCount;
	}
	ChangeMoney(bReachedGoal ? CompletedStage.Win : CompletedStage.Lose);
	ResetDiceAfterFinishedRound();
	ResetRoundScore();
	ResetRoundBoostState();

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
	if (Row->EffectOperation != EBoostEffectOperation::AddNextRoundPurchaseMultiplier)
	{
		for (const FOwnedBoostStack& Owned : OwnedBoosts)
		{
			const FBoostRow* OwnedRow = FindBoostRow(Owned.BoostId);
			if (OwnedRow && OwnedRow->EffectOperation == EBoostEffectOperation::AddNextRoundPurchaseMultiplier)
			{
				PendingReinvestmentMultiplier += FMath::RoundToInt(
					OwnedRow->EffectMagnitude * Owned.StackCount);
			}
		}
	}
	PendingAcceleratorPurchases += GetOwnedStacksForOperation(
		EBoostEffectOperation::MultiplyFirstCombinationAfterPurchase);
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
		ResetRoundBoostState();
		ActiveReinvestmentMultiplier = PendingReinvestmentMultiplier;
		PendingReinvestmentMultiplier = 0;
		ActiveAcceleratorPurchases = PendingAcceleratorPurchases;
		PendingAcceleratorPurchases = 0;
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
	PendingReinvestmentMultiplier = 0;
	PendingAcceleratorPurchases = 0;
	RunWinBoostCount = 0;
	CapitalismMoneyTriggerCount = 0;
	ResetRoundBoostState();
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
	Result.RetriggeredBaseScore = RetriggeredBaseScoreThisTurn;
	Result.RetriggerCount = RetriggerCountThisTurn;

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

	int32 ContextSetCount = 0;
	int32 ContextStraightCount = 0;
	int32 ContextFullStraightCount = 0;
	for (int32 Index = 0; Index < Context.CombinationTypes.Num(); ++Index)
	{
		if (Context.CombinationTypes[Index] == EDiceScoringCombinationType::SameFace)
		{
			++ContextSetCount;
		}
		else if (Context.CombinationTypes[Index] == EDiceScoringCombinationType::Straight)
		{
			++ContextStraightCount;
			if (Context.CombinationDiceCounts.IsValidIndex(Index)
				&& Context.CombinationDiceCounts[Index] == 6)
			{
				++ContextFullStraightCount;
			}
		}
	}

	// Первый проход меняет Base и добавляет уже заработанные Retrigger, независимо от порядка покупок.
	Result.ModifiedBaseScore += Result.RetriggeredBaseScore;
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
		else if (Row.EffectOperation == EBoostEffectOperation::AddBasePerSet)
		{
			Result.ModifiedBaseScore += FMath::RoundToInt(
				Row.EffectMagnitude * static_cast<float>(ContextSetCount * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddBasePerStraight)
		{
			Result.ModifiedBaseScore += FMath::RoundToInt(
				Row.EffectMagnitude * static_cast<float>(ContextStraightCount * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddBaseIfScoreAtLeast
			&& Context.BaseScore >= FMath::RoundToInt(Row.EffectThreshold))
		{
			Result.ModifiedBaseScore += FMath::RoundToInt(Row.EffectMagnitude * Effect.Stacks);
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
			const int32 EffectiveCombinationCount = FMath::Max(
				Context.CombinationCount + RetriggerCountThisTurn,
				CombinationCountThisRound + RetriggerCountThisTurn);
			Result.AdditiveMultiplier += Row.EffectMagnitude
				* static_cast<float>(EffectiveCombinationCount * Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerFaceMilestone)
		{
			int32 ContextMatches = 0;
			for (const int32 Face : Context.ScoredDiceValues)
			{
				ContextMatches += Face == Row.EffectFaceValue ? 1 : 0;
			}
			const int32 Count = Row.EffectFaceValue == 1
				? FMath::Max(ScoredOnesThisRound, ContextMatches)
				: FMath::Max(ScoredFivesThisRound, ContextMatches);
			const int32 Milestone = FMath::Max(1, FMath::RoundToInt(Row.EffectThreshold));
			Result.AdditiveMultiplier += Row.EffectMagnitude
				* static_cast<float>((Count / Milestone) * Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerPreviousSet)
		{
			const int32 Count = FMath::Max(SetCountThisRound, ContextSetCount);
			Result.AdditiveMultiplier += Row.EffectMagnitude
				* static_cast<float>(FMath::Max(0, Count - 1) * Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerHotDice)
		{
			const int32 Count = FMath::Max(HotDiceCountThisRound, Context.bIsHotDice ? 1 : 0);
			Result.AdditiveMultiplier += Row.EffectMagnitude * static_cast<float>(Count * Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerStraight)
		{
			const float Fallback = Row.EffectMagnitude * static_cast<float>(ContextStraightCount)
				+ (Row.EffectSecondaryMagnitude - Row.EffectMagnitude)
					* static_cast<float>(ContextFullStraightCount);
			Result.AdditiveMultiplier += FMath::Max(
				static_cast<float>(StraightMultiplierThisTurn), Fallback * Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerMoneyBlock)
		{
			const int32 Block = FMath::Max(1, FMath::RoundToInt(Row.EffectThreshold));
			Result.AdditiveMultiplier += Row.EffectMagnitude
				* static_cast<float>((FMath::Max(0, Money) / Block) * Effect.Stacks);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddNextRoundPurchaseMultiplier)
		{
			Result.AdditiveMultiplier += static_cast<float>(ActiveReinvestmentMultiplier);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerUniqueCombination)
		{
			int32 UniqueCount = CombinationRulesThisRound.Num();
			if (UniqueCount == 0)
			{
				TSet<FName> ContextRules;
				for (const FName RuleName : Context.CombinationRuleNames)
				{
					ContextRules.Add(RuleName);
				}
				UniqueCount = ContextRules.Num();
			}
			float Bonus = Row.EffectMagnitude * static_cast<float>(UniqueCount * Effect.Stacks);
			if (Row.EffectLimit > 0)
			{
				Bonus = FMath::Min(Bonus, static_cast<float>(Row.EffectLimit));
			}
			Result.AdditiveMultiplier += Bonus;
		}
		else if (Row.EffectOperation == EBoostEffectOperation::AddMultiplierPerSuccessfulReroll)
		{
			Result.AdditiveMultiplier += Row.EffectMagnitude
				* static_cast<float>(ConsecutiveSuccessfulRerolls * Effect.Stacks);
		}
	}

	// Третий проход собирает XMult-факторы. L07 последовательно усиливает каждый следующий.
	TArray<float> XMultiplierFactors;
	float XMultiplierEnhancement = 0.0f;
	for (const FActiveEffect& Effect : ActiveEffects)
	{
		const FBoostRow& Row = *Effect.Row;
		if (Row.EffectOperation == EBoostEffectOperation::MultiplyScore
			&& (Row.EffectThreshold <= 0.0f || Result.AdditiveMultiplier >= Row.EffectThreshold))
		{
			XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude), Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyPerOwnedBoost)
		{
			XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude),
				OwnedBoosts.Num() * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyPerSuccessAfterThreshold)
		{
			const int32 Exponent = ConsecutiveSuccessfulRolls
				- FMath::Max(1, FMath::RoundToInt(Row.EffectThreshold)) + 1;
			if (Exponent > 0)
			{
				XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude),
					Exponent * Effect.Stacks));
			}
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyNextFullCycleAfterHotDice
			&& CurrentRollHotCycleXMultiplier != 1.0f)
		{
			XMultiplierFactors.Add(CurrentRollHotCycleXMultiplier);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyPerMoneyBlock)
		{
			const int32 Block = FMath::Max(1, FMath::RoundToInt(Row.EffectThreshold));
			const int32 Exponent = (FMath::Max(0, Money) / Block) * Effect.Stacks;
			if (Exponent > 0)
			{
				XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude), Exponent));
			}
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyFirstCombinationAfterPurchase
			&& CurrentRollPurchaseXMultiplier != 1.0f)
		{
			XMultiplierFactors.Add(CurrentRollPurchaseXMultiplier);
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyPerSuccessfulRoll
			&& SuccessfulRollsThisTurn > 0)
		{
			XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude),
				SuccessfulRollsThisTurn * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyPerWin
			&& RunWinBoostCount > 0)
		{
			XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude),
				RunWinBoostCount * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyAfterUniqueCombinationTypes)
		{
			TSet<uint8> Types = CombinationTypesThisTurn;
			for (const EDiceScoringCombinationType Type : Context.CombinationTypes)
			{
				Types.Add(static_cast<uint8>(Type));
			}
			if (Types.Num() >= FMath::Max(1, FMath::RoundToInt(Row.EffectThreshold)))
			{
				XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude), Effect.Stacks));
			}
		}
		else if (Row.EffectOperation == EBoostEffectOperation::MultiplyPerBoostMoneyTrigger
			&& CapitalismMoneyTriggerCount > 0)
		{
			XMultiplierFactors.Add(FMath::Pow(FMath::Max(0.0f, Row.EffectMagnitude),
				CapitalismMoneyTriggerCount * Effect.Stacks));
		}
		else if (Row.EffectOperation == EBoostEffectOperation::EnhanceNextXMultiplier)
		{
			XMultiplierEnhancement += Row.EffectMagnitude * static_cast<float>(Effect.Stacks);
		}
	}
	for (int32 Index = 0; Index < XMultiplierFactors.Num(); ++Index)
	{
		const float EnhancedFactor = XMultiplierFactors[Index] + XMultiplierEnhancement
			* static_cast<float>(XMultiplierActivationsBeforeCurrentRoll + Index);
		Result.XMultiplier *= FMath::Max(0.0f, EnhancedFactor);
	}
	Result.ActivatedXMultiplierCount = XMultiplierFactors.Num();

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
	PendingReinvestmentMultiplier = 0;
	PendingAcceleratorPurchases = 0;
	ResetRoundBoostState();
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
			Context.CombinationTypes.Add(Combination.CombinationType);
			Context.CombinationRuleNames.Add(Combination.RuleRowName);
			Context.CombinationScores.Add(Combination.Score);
			Context.CombinationDiceCounts.Add(Combination.DiceValues.Num());
		}
		LastBoostEffectResult = EvaluateBoostEffects(Context);
		LastSelectionScoreResult.TotalScore = LastBoostEffectResult.FinalScore;
	}
	else
	{
		LastBoostEffectResult = FBoostEffectResult();
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
	case EBoostEffectTrigger::RollResolved:
	case EBoostEffectTrigger::RoundFinished:
	case EBoostEffectTrigger::BoostPurchased:
	case EBoostEffectTrigger::RunWon:
		return false;
	default:
		return false;
	}
}

/** Суммирует стаки по operation, сохраняя весь баланс и MaxStacks в DT_Boosts. */
int32 UGameManagerSubsystem::GetOwnedStacksForOperation(const EBoostEffectOperation Operation) const
{
	int32 TotalStacks = 0;
	for (const FOwnedBoostStack& Owned : OwnedBoosts)
	{
		const FBoostRow* Row = FindBoostRow(Owned.BoostId);
		if (Row && Row->EffectOperation == Operation)
		{
			TotalStacks += Owned.StackCount;
		}
	}
	return TotalStacks;
}

/** Обнуляет память текущего раунда, не затрагивая run-wide победы и магазинные pending-заряды. */
void UGameManagerSubsystem::ResetRoundBoostState()
{
	ScoredOnesThisRound = 0;
	ScoredFivesThisRound = 0;
	CombinationCountThisRound = 0;
	SetCountThisRound = 0;
	HotDiceCountThisRound = 0;
	SuccessfulRollsThisTurn = 0;
	ConsecutiveSuccessfulRolls = 0;
	ConsecutiveSuccessfulRerolls = 0;
	StraightMultiplierThisTurn = 0;
	RetriggeredBaseScoreThisTurn = 0;
	RetriggerCountThisTurn = 0;
	LastScoringCombinationScore = 0;
	PendingHotDiceFullCycles = 0;
	ActiveReinvestmentMultiplier = 0;
	ActiveAcceleratorPurchases = 0;
	RoundXMultiplierActivations = 0;
	XMultiplierActivationsBeforeCurrentRoll = 0;
	CurrentRollHotCycleXMultiplier = 1.0f;
	CurrentRollPurchaseXMultiplier = 1.0f;
	bHasResolvedRollThisRound = false;
	bCurrentRollIsReroll = false;
	bHotDiceBeforeCurrentRoll = false;
	bRecursiveRetriggerUsedThisTurn = false;
	CombinationRulesThisRound.Reset();
	CombinationTypesThisTurn.Reset();
}

/** R10 выдаёт монеты по итоговому порогу, а L08 запоминает каждый coin как отдельную активацию. */
void UGameManagerSubsystem::GrantScoreBoostMoney(const int32 FinishedScore)
{
	for (const FOwnedBoostStack& Owned : OwnedBoosts)
	{
		const FBoostRow* Row = FindBoostRow(Owned.BoostId);
		if (!Row || Row->EffectOperation != EBoostEffectOperation::GrantMoneyPerScoreBlock)
		{
			continue;
		}
		const int32 ScoreBlock = FMath::Max(1, FMath::RoundToInt(Row->EffectThreshold));
		int32 Coins = FMath::Max(0, FinishedScore) / ScoreBlock;
		Coins *= FMath::Max(0, FMath::RoundToInt(Row->EffectMagnitude)) * Owned.StackCount;
		if (Row->EffectLimit > 0)
		{
			Coins = FMath::Min(Coins, Row->EffectLimit);
		}
		if (Coins > 0)
		{
			ChangeMoney(Coins);
			if (GetOwnedStacksForOperation(EBoostEffectOperation::MultiplyPerBoostMoneyTrigger) > 0)
			{
				CapitalismMoneyTriggerCount += Coins;
			}
		}
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
