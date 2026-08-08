// Fill out your copyright notice in the Description page of Project Settings.


#include "Dice/CPP_Dice.h"

#include "Components/TimelineComponent.h"

// Sets default values
ACPP_Dice::ACPP_Dice()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	SMC_Dice = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiceStaticMeshComponent"));
	MaterialTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MaterialTimeline"));
}

// Called when the game starts or when spawned
void ACPP_Dice::BeginPlay()
{
	Super::BeginPlay();
	
	DynMaterial = SMC_Dice->CreateAndSetMaterialInstanceDynamic(0);
}

void ACPP_Dice::UpdateMaterialParameter(float Value)
{
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(FName("Disolve"), Value);
	}
}

// Called every frame
void ACPP_Dice::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ACPP_Dice::ShowDiceEffect(bool bDissolve)
{
	FOnTimelineFloat TimelineProgress;
	TimelineProgress.BindUFunction(this, FName("UpdateMaterialParameter"));
	if (bDissolve)
	{
		
		if (DissolveMaterial != nullptr)
		{
			SMC_Dice->SetMaterial(0, DissolveMaterial);
        
			// CRITICAL: We must recreate the dynamic material instance based on the NEW material
			DynMaterial = SMC_Dice->CreateAndSetMaterialInstanceDynamic(0);
		}
		
		
		if (MaterialTimeline && DissolveTransitionCurve)
		{
			MaterialTimeline->AddInterpFloat(DissolveTransitionCurve, TimelineProgress, NAME_None, FName("MatTransitionTrack"));
		}
	}
	else
	{
		if (AppearMaterial != nullptr)
		{
			SMC_Dice->SetMaterial(0, AppearMaterial);
        
			// CRITICAL: We must recreate the dynamic material instance based on the NEW material
			DynMaterial = SMC_Dice->CreateAndSetMaterialInstanceDynamic(0);
		}
		
		if (MaterialTimeline && AppearTransitionCurve)
		{
			MaterialTimeline->AddInterpFloat(AppearTransitionCurve, TimelineProgress, NAME_None, FName("MatTransitionTrack"));
		}
	}
	
	MaterialTimeline->PlayFromStart();
}



