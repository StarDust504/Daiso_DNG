// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/GameManagerSubsystem.h"

#include "Dice/CPP_Dice.h"
#include "Dice/DiceScoringLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"

namespace DiceSelection
{
	// Преобразует массив значений в компактную строку для отладочного сообщения.
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

// Загружает стандартную таблицу правил при создании подсистемы мира.
void UGameManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DiceScoringRules = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	LevelGoalsTable = LoadObject<UDataTable>(
		nullptr, TEXT("/Game/Data/DT_LevelGoals.DT_LevelGoals"));
	LoadCurrentLevelGoal();
}

// Добавляет значение выбранного кубика и сразу пересчитывает текущую комбинацию.
FName UGameManagerSubsystem::AddComboToTempArray(const int32 NumberToAppend)
{
	TempScore.Add(NumberToAppend);
	TempScore.Sort();
	RefreshSelectionScore();
	return BuildSelectedDiceKey();
}

// Удаляет одно совпадающее значение кубика и пересчитывает оставшийся выбор.
FName UGameManagerSubsystem::RemoveComboFromTempArray(const int32 NumberToRemove)
{
	if (!TempScore.IsEmpty() && TempScore.Contains(NumberToRemove))
	{
		TempScore.RemoveSingle(NumberToRemove);
		TempScore.Sort();
	}
	RefreshSelectionScore();
	return BuildSelectedDiceKey();
}

// Возвращает текущий счёт; старый параметр ключа оставлен ради совместимости Blueprint.
int32 UGameManagerSubsystem::GetCurrentScore(FName)
{
	return LastSelectionScoreResult.TotalScore;
}

// Возвращает копию отсортированных значений выбранных кубиков.
TArray<int32> UGameManagerSubsystem::GetSelectedDiceValues() const
{
	return TempScore;
}

// Возвращает полный результат последнего пересчёта выбранных кубиков.
FDiceRollScoreResult UGameManagerSubsystem::GetSelectedDiceScore() const
{
	return LastSelectionScoreResult;
}

// Сообщает, что все выбранные кубики можно полностью разложить на результативные комбинации.
bool UGameManagerSubsystem::IsCurrentDiceSelectionValid() const
{
	return bIsCurrentSelectionValid;
}

// Очищает выбор и публикует пустой результат подписчикам.
void UGameManagerSubsystem::ClearDiceSelection()
{
	TempScore.Reset();
	RefreshSelectionScore();
}

// Собирает единый снимок динамического счёта сохранённых костей и текущей цели уровня.
FLevelProgressState UGameManagerSubsystem::GetLevelProgress() const
{
	FLevelProgressState State;
	State.LevelNumber = CurrentLevelNumber;
	State.TargetScore = CurrentLevelTargetScore;
	State.CurrentScore = LastSelectionScoreResult.TotalScore;
	State.bCanFinishRound = !bLevelWon
		&& bIsCurrentSelectionValid
		&& State.CurrentScore > 0;
	State.bLevelWon = bLevelWon;
	return State;
}

// Завершает валидный раунд, сбрасывает его счёт и при успехе переключает подсистему на следующую цель.
bool UGameManagerSubsystem::FinishRound()
{
	FLevelProgressState FinishedRound = GetLevelProgress();
	if (!FinishedRound.bCanFinishRound)
	{
		return false;
	}

	const bool bReachedGoal = FinishedRound.CurrentScore >= CurrentLevelTargetScore;
	FinishedRound.bLevelWon = bReachedGoal;
	ResetDiceAfterFinishedRound();
	TempScore.Reset();
	LastSelectionScoreResult = FDiceRollScoreResult();
	bIsCurrentSelectionValid = false;
	bLevelWon = false;

	if (bReachedGoal)
	{
		const int32 CompletedLevelNumber = CurrentLevelNumber;
		const int32 CompletedTargetScore = CurrentLevelTargetScore;
		++CurrentLevelNumber;
		if (!LoadCurrentLevelGoal())
		{
			CurrentLevelNumber = CompletedLevelNumber;
			CurrentLevelTargetScore = CompletedTargetScore;
			bLevelWon = true;
		}
	}

	OnDiceSelectionChanged.Broadcast(LastSelectionScoreResult);
	PublishLevelProgress();
	if (bReachedGoal)
	{
		OnLevelWon.Broadcast(FinishedRound);
	}
	return true;
}

// Загружает новую цель по номеру уровня и начинает её с чистого счёта.
bool UGameManagerSubsystem::SetCurrentLevelNumber(const int32 NewLevelNumber)
{
	if (NewLevelNumber < 1)
	{
		return false;
	}

	const int32 PreviousLevelNumber = CurrentLevelNumber;
	const int32 PreviousTargetScore = CurrentLevelTargetScore;
	CurrentLevelNumber = NewLevelNumber;
	if (!LoadCurrentLevelGoal())
	{
		CurrentLevelNumber = PreviousLevelNumber;
		CurrentLevelTargetScore = PreviousTargetScore;
		return false;
	}

	ResetDiceAfterFinishedRound();
	TempScore.Reset();
	LastSelectionScoreResult = FDiceRollScoreResult();
	bIsCurrentSelectionValid = false;
	bLevelWon = false;
	OnDiceSelectionChanged.Broadcast(LastSelectionScoreResult);
	PublishLevelProgress();
	return true;
}

// Собирает совместимый FName-ключ из отсортированных значений выбранных кубиков.
FName UGameManagerSubsystem::BuildSelectedDiceKey() const
{
	FString Key;
	for (const int32 Value : TempScore)
	{
		Key.AppendInt(Value);
	}
	return Key.IsEmpty() ? NAME_None : FName(*Key);
}

// Пересчитывает выбор по Data Table, обновляет валидность и выводит диагностику.
void UGameManagerSubsystem::RefreshSelectionScore()
{
	if (!IsValid(DiceScoringRules))
	{
		DiceScoringRules = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	}

	LastSelectionScoreResult = TempScore.IsEmpty()
		? FDiceRollScoreResult()
		: UDiceScoringLibrary::CalculateSelectedDiceScore(TempScore, DiceScoringRules);
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

// Читает строку Level_XX из отдельной таблицы целей и обновляет активную цель.
bool UGameManagerSubsystem::LoadCurrentLevelGoal()
{
	if (!IsValid(LevelGoalsTable))
	{
		LevelGoalsTable = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Data/DT_LevelGoals.DT_LevelGoals"));
	}
	if (!IsValid(LevelGoalsTable))
	{
		UE_LOG(LogTemp, Warning, TEXT("DT_LevelGoals is not available; using fallback target %d."),
			CurrentLevelTargetScore);
		return CurrentLevelNumber == 1;
	}

	const FName RowName(*FString::Printf(TEXT("Level_%02d"), CurrentLevelNumber));
	const FLevelGoalRow* Goal = LevelGoalsTable->FindRow<FLevelGoalRow>(
		RowName, TEXT("GameManagerSubsystem::LoadCurrentLevelGoal"), false);
	if (!Goal || Goal->LevelNumber != CurrentLevelNumber || Goal->TargetScore < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("DT_LevelGoals has no valid row for level %d."), CurrentLevelNumber);
		return false;
	}

	CurrentLevelTargetScore = Goal->TargetScore;
	return true;
}

// Рассылает подписчикам актуальное состояние прогресса уровня.
void UGameManagerSubsystem::PublishLevelProgress()
{
	OnLevelProgressChanged.Broadcast(GetLevelProgress());
}

// Возвращает выбранные кубики в обычное состояние и очищает их внутренний реестр.
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

// Добавляет валидный кубик в реестр без дубликатов.
void UGameManagerSubsystem::RegisterDice(ACPP_Dice* DiceToRegister)
{
	if (!IsValid(DiceToRegister) || RegisteredDice.Contains(DiceToRegister))
	{
		return;
	}
	
	DiceToRegister->SetCanRollDice(false);
	
	RegisteredDice.Add(DiceToRegister);
}

// Удаляет кубик из реестра, если он был зарегистрирован.
void UGameManagerSubsystem::UnregisterDice(ACPP_Dice* DiceToUnregister)
{
	if (!RegisteredDice.Contains(DiceToUnregister))
	{
		return;
	}
	
	DiceToUnregister->SetCanRollDice(true);
	
	RegisteredDice.Remove(DiceToUnregister);
}

// Проверяет наличие кубика в реестре подсистемы.
bool UGameManagerSubsystem::CheckIsDiceRegistered(ACPP_Dice* DiceToCheck) const
{
	return RegisteredDice.Contains(DiceToCheck);
}

void UGameManagerSubsystem::DestroyRegisteredDice()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, FString::FromInt(RegisteredDice.Num()));
	if (RegisteredDice.Num() < 6)
	{
		for (ACPP_Dice* Dice : RegisteredDice)
		{
			if (!Dice->bIsHidden)
			{
				Dice->ShowDiceEffect(true);
				Dice->bIsHidden = true;
			}
			
		}
	}
	else
	{
		for (int i = 0; i < 6; i++)
		{
			if (RegisteredDice[i]->bIsHidden)
			{
				RegisteredDice[i]->ShowDiceEffect(false);
				RegisteredDice[i]->bIsHidden = false;
			}
			
			RegisteredDice[i]->SetCanRollDice(true);
			RegisteredDice[i]->SetIsActive(false);
		}
		
		RegisteredDice[5]->SetCanRollDice(true);
		RegisteredDice[5]->SetIsActive(false);
		
		RegisteredDice.Empty();
	}
}

// Сохраняет ссылку на старую таблицу очков для существующей Blueprint-логики.
void UGameManagerSubsystem::RegisterScoreDataTable(UDataTable* ScoreDT)
{
	ScoreDataTable = ScoreDT;
}

