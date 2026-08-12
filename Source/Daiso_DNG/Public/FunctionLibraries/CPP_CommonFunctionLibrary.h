// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CPP_CommonFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class DAISO_DNG_API UCPP_CommonFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	template<typename T>
	static TArray<T*> GetAllActorsOfClass(const UObject* WorldContextObject)
	{
		TArray<T*> FoundActors;

		// Safety check for the context object
		if (!WorldContextObject)
		{
			return FoundActors;
		}

		// Get the world from the passed-in context object
		UWorld* World = WorldContextObject->GetWorld();
		if (!World)
		{
			return FoundActors;
		}

		// Iterate through the world
		for (TActorIterator<T> It(World); It; ++It)
		{
			T* Actor = *It;
			if (Actor)
			{
				FoundActors.Add(Actor);
			}
		}

		return FoundActors;
	}
	
	template<typename T>
	static T* GetActorOfClass(const UObject* WorldContextObject)
	{
		T* FoundActor = nullptr;
		
		UWorld* World = WorldContextObject->GetWorld();
		if (!World)
		{
			return FoundActor;
		}
		
		for (TActorIterator<T> It(World); It; ++It)
		{
			FoundActor = *It;
			if (FoundActor)
			{
				return FoundActor;
			}
		}
		
		return FoundActor;
	}
};
