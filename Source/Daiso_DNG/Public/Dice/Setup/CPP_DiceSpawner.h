// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_DiceSpawner.generated.h"


UENUM(BlueprintType)
enum class ESpawnerType : uint8
{
	PLAYER UMETA(DisplayName="Player"),
	AI UMETA(DisplayName="AI"),
	BOARD UMETA(DisplayName="Board"),
};
UCLASS()
class DAISO_DNG_API ACPP_DiceSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACPP_DiceSpawner();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	ESpawnerType SpawnerType;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
