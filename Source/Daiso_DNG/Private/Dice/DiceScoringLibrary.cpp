// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DiceScoringLibrary.h"

#include "Engine/DataTable.h"

namespace DiceScoring
{
	constexpr int32 DicePerRoll = 6;
	constexpr int32 FullMask = (1 << DicePerRoll) - 1;

	struct FCandidate
	{
		int32 Mask = 0;
		int32 Score = 0;
		int32 Priority = 0;
		FName RuleName = NAME_None;
		EDiceScoringCombinationType Type = EDiceScoringCombinationType::SingleFace;
		TArray<int32> Values;
	};

	struct FPartition
	{
		bool bReachable = false;
		int32 Score = 0;
		int32 Priority = 0;
		int32 ScoredDice = 0;
		TArray<int32> CandidateIndices;
	};

	// Проверяет базовую целостность строки Data Table перед созданием комбинаций.
	static bool IsRuleUsable(const FDiceScoringRule& Rule)
	{
		if (Rule.BaseScore <= 0 || Rule.MinDiceCount < 1 || Rule.MaxDiceCount > DicePerRoll
			|| Rule.MinDiceCount > Rule.MaxDiceCount)
		{
			return false;
		}

		if (Rule.CombinationType == EDiceScoringCombinationType::Straight)
		{
			const int32 StraightLength = Rule.StraightEnd - Rule.StraightStart + 1;
			return Rule.StraightStart >= 1 && Rule.StraightEnd <= 6 && StraightLength >= 1
				&& StraightLength >= Rule.MinDiceCount && StraightLength <= Rule.MaxDiceCount;
		}

		return Rule.FaceValue >= 1 && Rule.FaceValue <= 6;
	}

	// Проверяет, соответствует ли конкретное подмножество кубиков заданному правилу.
	static bool MatchesSubset(const FDiceScoringRule& Rule, const TArray<int32>& Values)
	{
		if (Values.Num() < Rule.MinDiceCount || Values.Num() > Rule.MaxDiceCount)
		{
			return false;
		}

		if (Rule.CombinationType != EDiceScoringCombinationType::Straight)
		{
			for (const int32 Value : Values)
			{
				if (Value != Rule.FaceValue)
				{
					return false;
				}
			}
			return true;
		}

		const int32 StraightLength = Rule.StraightEnd - Rule.StraightStart + 1;
		if (Values.Num() != StraightLength)
		{
			return false;
		}

		bool SeenFaces[6] = {};
		for (const int32 Value : Values)
		{
			if (Value < Rule.StraightStart || Value > Rule.StraightEnd || SeenFaces[Value - 1])
			{
				return false;
			}
			SeenFaces[Value - 1] = true;
		}
		return true;
	}

	// Рассчитывает стоимость комбинации с учётом правила масштабирования дополнительных кубиков.
	static int32 CalculateRuleScore(const FDiceScoringRule& Rule, const int32 DiceCount)
	{
		if (Rule.ScalingRule == EDiceScoreScalingRule::None)
		{
			return Rule.BaseScore;
		}

		const int32 ExtraDice = FMath::Max(0, DiceCount - Rule.MinDiceCount);
		return static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(Rule.BaseScore) << ExtraDice,
			MAX_int32));
	}

	// Сравнивает два разбиения по очкам, приоритету, числу использованных костей и компактности.
	static bool IsBetter(const FPartition& Candidate, const FPartition& Existing)
	{
		if (!Existing.bReachable || Candidate.Score != Existing.Score)
		{
			return !Existing.bReachable || Candidate.Score > Existing.Score;
		}
		if (Candidate.Priority != Existing.Priority)
		{
			return Candidate.Priority > Existing.Priority;
		}
		if (Candidate.ScoredDice != Existing.ScoredDice)
		{
			return Candidate.ScoredDice > Existing.ScoredDice;
		}
		return Candidate.CandidateIndices.Num() < Existing.CandidateIndices.Num();
	}
}

// Перебирает все допустимые комбинации и динамически выбирает самое выгодное непересекающееся разбиение.
FDiceRollScoreResult UDiceScoringLibrary::CalculateSelectedDiceScore(
	const TArray<int32>& SelectedDiceValues,
	UDataTable* ScoringRules)
{
	using namespace DiceScoring;

	FDiceRollScoreResult Result;
	if (SelectedDiceValues.IsEmpty() || SelectedDiceValues.Num() > DicePerRoll)
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("Expected from 1 to 6 selected dice, received %d."), SelectedDiceValues.Num());
		return Result;
	}
	for (const int32 Value : SelectedDiceValues)
	{
		if (Value < 1 || Value > 6)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Die value %d is outside the valid range 1..6."), Value);
			return Result;
		}
	}
	if (!IsValid(ScoringRules) || !IsValid(ScoringRules->GetRowStruct())
		|| !ScoringRules->GetRowStruct()->IsChildOf(FDiceScoringRule::StaticStruct()))
	{
		Result.ErrorMessage = TEXT("Scoring Rules must be a Data Table based on DiceScoringRule.");
		return Result;
	}
	const int32 InputDiceCount = SelectedDiceValues.Num();
	const int32 InputFullMask = (1 << InputDiceCount) - 1;

	TArray<FCandidate> Candidates;
	for (const TPair<FName, uint8*>& RowPair : ScoringRules->GetRowMap())
	{
		const FDiceScoringRule* Rule = reinterpret_cast<const FDiceScoringRule*>(RowPair.Value);
		if (!Rule || !IsRuleUsable(*Rule))
		{
			continue;
		}

		for (int32 Mask = 1; Mask <= InputFullMask; ++Mask)
		{
			TArray<int32> Subset;
			for (int32 Index = 0; Index < InputDiceCount; ++Index)
			{
				if ((Mask & (1 << Index)) != 0)
				{
					Subset.Add(SelectedDiceValues[Index]);
				}
			}
			if (MatchesSubset(*Rule, Subset))
			{
				FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.Mask = Mask;
				Candidate.Score = CalculateRuleScore(*Rule, Subset.Num());
				Candidate.Priority = Rule->Priority;
				Candidate.RuleName = RowPair.Key;
				Candidate.Type = Rule->CombinationType;
				Candidate.Values = MoveTemp(Subset);
			}
		}
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.Mask != B.Mask)
		{
			return A.Mask < B.Mask;
		}
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}
		return A.RuleName.LexicalLess(B.RuleName);
	});

	FPartition Partitions[FullMask + 1];
	Partitions[0].bReachable = true;
	for (int32 UsedMask = 0; UsedMask <= InputFullMask; ++UsedMask)
	{
		if (!Partitions[UsedMask].bReachable)
		{
			continue;
		}
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			const FCandidate& Candidate = Candidates[CandidateIndex];
			if ((UsedMask & Candidate.Mask) != 0)
			{
				continue;
			}

			FPartition Next = Partitions[UsedMask];
			Next.Score += Candidate.Score;
			Next.Priority += Candidate.Priority;
			Next.ScoredDice += Candidate.Values.Num();
			Next.CandidateIndices.Add(CandidateIndex);
			const int32 NextMask = UsedMask | Candidate.Mask;
			if (IsBetter(Next, Partitions[NextMask]))
			{
				Partitions[NextMask] = MoveTemp(Next);
			}
		}
	}

	int32 BestMask = 0;
	for (int32 Mask = 1; Mask <= InputFullMask; ++Mask)
	{
		if (Partitions[Mask].bReachable && IsBetter(Partitions[Mask], Partitions[BestMask]))
		{
			BestMask = Mask;
		}
	}

	Result.bIsValid = true;
	Result.TotalScore = Partitions[BestMask].Score;
	Result.bIsBust = Result.TotalScore == 0;
	for (const int32 CandidateIndex : Partitions[BestMask].CandidateIndices)
	{
		const FCandidate& Candidate = Candidates[CandidateIndex];
		FDiceScoringCombination& Combination = Result.Combinations.AddDefaulted_GetRef();
		Combination.RuleRowName = Candidate.RuleName;
		Combination.CombinationType = Candidate.Type;
		Combination.DiceValues = Candidate.Values;
		Combination.Score = Candidate.Score;
	}
	for (int32 Index = 0; Index < InputDiceCount; ++Index)
	{
		if ((BestMask & (1 << Index)) == 0)
		{
			Result.UnscoredDiceValues.Add(SelectedDiceValues[Index]);
		}
	}
	Result.bAllDiceScored = Result.TotalScore > 0 && Result.UnscoredDiceValues.IsEmpty();
	return Result;
}

// Проверяет наличие ровно шести кубиков и передаёт их универсальному подсчёту выбранного набора.
FDiceRollScoreResult UDiceScoringLibrary::CalculateDiceRollScore(
	const TArray<int32>& DiceValues,
	UDataTable* ScoringRules)
{
	if (DiceValues.Num() != DiceScoring::DicePerRoll)
	{
		FDiceRollScoreResult Result;
		Result.ErrorMessage = FString::Printf(TEXT("Expected exactly 6 dice, received %d."), DiceValues.Num());
		return Result;
	}
	return CalculateSelectedDiceScore(DiceValues, ScoringRules);
}
