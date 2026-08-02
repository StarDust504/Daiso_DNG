// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/GameManagerSubsystem.h"

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

int32 UGameManagerSubsystem::GetCurrentScore(FName Combo)
{
	return 0;
}


