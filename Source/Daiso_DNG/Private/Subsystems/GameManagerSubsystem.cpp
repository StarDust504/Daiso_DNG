// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/GameManagerSubsystem.h"
#include "Daiso_DNG/Public/Data/DataStructs.h"

FName UGameManagerSubsystem::AddComboToTempArray(int32 NumberToAppend)
{
	TempScore.Add(NumberToAppend);
	
	FString TempString;
	
	TempScore.Sort();
	
	for (int32 ArrNum : TempScore)
	{
		TempString.Append(FString::FromInt(ArrNum));
	}
	
	return FName(*TempString);
}

FName UGameManagerSubsystem::RemoveComboFromTempArray(int32 NumberToRemove)
{
	FString TempString;
	
	if (!TempScore.IsEmpty() && TempScore.Contains(NumberToRemove))
	{
		TempScore.RemoveSingle(NumberToRemove);
		
		TempScore.Sort();
	}
	
	for (int32 ArrNum : TempScore)
	{
		TempString.Append(FString::FromInt(ArrNum));
	}
	
	return FName(*TempString);
}

int32 UGameManagerSubsystem::GetCurrentScore(FName Combo)
{
	if (!ScoreDataTable)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, "DT not found!!!");
		return -1;
	}
		
	
	FComboData* FoundRow = ScoreDataTable->FindRow<FComboData>(Combo, "");
	if (!FoundRow) return 0;
	
	return FoundRow->ComboScore;
}

void UGameManagerSubsystem::RegisterDice(ACPP_Dice* DiceToRegister)
{
	if (RegisteredDice.Contains(DiceToRegister))
		return;
	
	RegisteredDice.Add(DiceToRegister);
}

void UGameManagerSubsystem::UnregisterDice(ACPP_Dice* DiceToUnregister)
{
	if (!RegisteredDice.Contains(DiceToUnregister))
		return;
	
	RegisteredDice.Remove(DiceToUnregister);
}


