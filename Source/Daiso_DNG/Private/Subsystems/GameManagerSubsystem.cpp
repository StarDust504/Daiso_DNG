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

