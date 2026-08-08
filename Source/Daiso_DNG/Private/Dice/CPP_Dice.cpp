// Fill out your copyright notice in the Description page of Project Settings.


#include "Dice/CPP_Dice.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"

// Sets default values
ACPP_Dice::ACPP_Dice()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	SMC_Dice = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiceStaticMeshComponent"));
	SetRootComponent(SMC_Dice);
	MaterialTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MaterialTimeline"));
	SelectionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SelectionLight"));
	SelectionLight->SetupAttachment(SMC_Dice);
	SelectionLight->SetRelativeLocation(FVector(0.0, 0.0, -2.6));
	SelectionLight->SetMobility(EComponentMobility::Movable);
	SelectionLight->SetLightColor(FLinearColor(0.08f, 0.32f, 0.06f));
	SelectionLight->SetIntensity(220.0f);
	SelectionLight->SetAttenuationRadius(38.0f);
	SelectionLight->SetSourceRadius(3.0f);
	SelectionLight->SetCastShadows(false);
	SelectionLight->bUseInverseSquaredFalloff = false;
	SelectionLight->LightFalloffExponent = 6.0f;
	SelectionLight->SetVolumetricScatteringIntensity(0.0f);
	SelectionLight->SetVisibility(false);
}

// Called when the game starts or when spawned
void ACPP_Dice::BeginPlay()
{
	Super::BeginPlay();
	
	DynMaterial = SMC_Dice->CreateAndSetMaterialInstanceDynamic(0);
	ApplySelectionHighlight();
}

// Меняет логическое состояние выбора и немедленно обновляет свет под кубиком.
void ACPP_Dice::SetIsActive(const bool NewActive)
{
	bIsActive = NewActive;
	ApplySelectionHighlight();
}

// Не затрагивает материал кубика: включает только отдельный источник света под ним.
void ACPP_Dice::ApplySelectionHighlight()
{
	if (IsValid(SelectionLight))
	{
		SelectionLight->SetVisibility(bIsActive && !bIsHidden);
	}
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
	if (IsValid(SelectionLight))
	{
		SelectionLight->SetVisibility(!bDissolve && bIsActive);
	}

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



