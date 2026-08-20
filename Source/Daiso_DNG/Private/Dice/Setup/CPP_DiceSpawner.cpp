// Fill out your copyright notice in the Description page of Project Settings.


#include "Dice/Setup/CPP_DiceSpawner.h"

// Sets default values
ACPP_DiceSpawner::ACPP_DiceSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACPP_DiceSpawner::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACPP_DiceSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

