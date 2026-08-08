// Fill out your copyright notice in the Description page of Project Settings.


#include "Dice/AI/DiceEnemyAI.h"

// Sets default values
ADiceEnemyAI::ADiceEnemyAI()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ADiceEnemyAI::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADiceEnemyAI::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

