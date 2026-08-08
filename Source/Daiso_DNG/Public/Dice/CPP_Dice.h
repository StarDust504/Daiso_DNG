// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_Dice.generated.h"

class UTimelineComponent;
class UMaterialInterface;
UCLASS()
class DAISO_DNG_API ACPP_Dice : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACPP_Dice();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* SMC_Dice;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	UMaterialInterface* DissolveMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	UMaterialInterface* AppearMaterial;

	/** Материал контура, который накладывается поверх выбранного кубика. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> SelectionHighlightMaterial = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTimelineComponent* MaterialTimeline;
	
	UPROPERTY(EditAnywhere, Category = "Material Transition")
	class UCurveFloat* DissolveTransitionCurve;
	
	UPROPERTY(EditAnywhere, Category = "Material Transition")
	class UCurveFloat* AppearTransitionCurve;

	UPROPERTY()
	class UMaterialInstanceDynamic* DynMaterial;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void UpdateMaterialParameter(float Value);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Visuals")
	void ShowDiceEffect(bool bDissolve);
	
	// SETTER: Changes data, returns void -> Use BlueprintCallable ONLY
	UFUNCTION(BlueprintCallable, Category = "Game")
	FORCEINLINE void SetCanRollDice(bool NewCanRoll){ bCanRoll = NewCanRoll; };
    
	// GETTER: Reads data, returns bool -> Use BlueprintPure ONLY
	UFUNCTION(BlueprintPure, Category = "Game")
	FORCEINLINE bool GetCanRollDice(){ return bCanRoll; };
    
	/** Меняет состояние выбора и сразу синхронизирует визуальную подсветку кубика. */
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetIsActive(bool NewActive);
    
	// GETTER: Reads data, returns bool -> Use BlueprintPure ONLY
	UFUNCTION(BlueprintPure, Category = "Game")
	FORCEINLINE bool GetIsActive(){ return bIsActive; };
	
	bool bIsHidden = false;
	
private:
	/** Включает или снимает overlay-материал в соответствии с текущим состоянием выбора. */
	void ApplySelectionHighlight();

	bool bCanRoll = true;
	bool bIsActive = false;
};
