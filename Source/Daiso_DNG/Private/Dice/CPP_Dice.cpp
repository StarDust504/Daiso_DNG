// Fill out your copyright notice in the Description page of Project Settings.


#include "Dice/CPP_Dice.h"

// Sets default values
ACPP_Dice::ACPP_Dice()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACPP_Dice::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACPP_Dice::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

