// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MouseGatherDiceManager.generated.h"

class ACPP_Dice;
class UBoxComponent;
class UDicePhysicsRollComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EMouseGatherDropMode : uint8
{
	Natural UMETA(DisplayName="Physical outcome"),
	Predicted UMETA(DisplayName="Predetermined outcome")
};

UENUM(BlueprintType)
enum class EMouseGatherState : uint8
{
	Idle,
	Gathering,
	NaturalDropping,
	PredictedDropping
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMouseGatherStatusSignature, FText, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FMouseGatherFinishedSignature, const TArray<int32>&, Results);

/**
 * Lets the player sweep the cursor over dice, carry them as an elevated group, and release them with LMB.
 * The two drop modes share the interaction but deliberately use different result semantics.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API AMouseGatherDiceManager : public AActor
{
	GENERATED_BODY()

public:
	AMouseGatherDiceManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Starts or switches the cursor-gather mode. The button's own click is ignored until it has been released. */
	UFUNCTION(BlueprintCallable, Category="Dice|Mouse Gather")
	bool BeginGather(EMouseGatherDropMode NewMode);

	/**
	 * Начинает ручной сбор с бросковым жестом: после сбора удерживайте ЛКМ,
	 * проведите пригоршню по траектории и отпустите кнопку.
	 */
	UFUNCTION(BlueprintCallable, Category="Dice|Mouse Gather")
	bool BeginTrajectoryGather(EMouseGatherDropMode NewMode = EMouseGatherDropMode::Natural);

	/** Adds a die to the carried group. Public for Blueprint extensions and deterministic automation tests. */
	UFUNCTION(BlueprintCallable, Category="Dice|Mouse Gather")
	bool TryGatherDice(AActor* DiceActor);

	/** Drops the currently gathered group using the selected result mode. */
	UFUNCTION(BlueprintCallable, Category="Dice|Mouse Gather")
	bool DropGatheredDice();

	/** Выпускает собранные кубики с общей мировой скоростью; используется жестом и Blueprint-вариантами. */
	UFUNCTION(BlueprintCallable, Category="Dice|Mouse Gather")
	bool LaunchGatheredDice(FVector ThrowVelocity);

	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	EMouseGatherState GetInteractionState() const { return InteractionState; }

	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	EMouseGatherDropMode GetDropMode() const { return DropMode; }

	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	int32 GetGatheredDiceCount() const { return GatheredDice.Num(); }

	/** Кубики последней завершённой группы в том же порядке, что и результаты OnFinished. */
	TArray<ACPP_Dice*> GetLastFinishedDice() const;

	/** Board impacts which played guided-roll sound/camera feedback during the last honest gathered drop. */
	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	int32 GetLastNaturalDropImpactFeedbackCount() const { return LastNaturalDropImpactFeedbackCount; }

	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	bool IsInteractionActive() const { return InteractionState != EMouseGatherState::Idle; }

	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	bool IsTrajectoryGestureActive() const { return bTrajectoryGestureActive; }

	UFUNCTION(BlueprintPure, Category="Dice|Mouse Gather")
	bool IsDropping() const
	{
		return InteractionState == EMouseGatherState::NaturalDropping
			|| InteractionState == EMouseGatherState::PredictedDropping;
	}

	/** Height of the carried dice centres over the playable surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="10.0", UIMin="10.0", UIMax="150.0"))
	float HoverHeight = 58.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="5.0", UIMin="5.0", UIMax="30.0"))
	float GatherSpacing = 6.2f;

	/** Strength of the invisible springs which pull carried dice into a compact cloud. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="1.0", UIMin="1.0", UIMax="120.0"))
	float CarrySpringStrength = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0"))
	float CarrySpringDamping = 12.0f;

	/** Small independent motion which keeps the compact group feeling alive in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="5.0"))
	float CarryFloatAmplitude = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="5.0"))
	float CarryTumbleSpeed = 0.85f;

	/** Cursor distance in screen pixels which counts as sweeping over a die. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Carry",
		meta=(ClampMin="4.0", UIMin="4.0", UIMax="100.0"))
	float PickupScreenRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Drop",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="100.0"))
	float NaturalHorizontalKick = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Drop",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="80.0"))
	float NaturalSpinSpeed = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Drop",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="300.0"))
	float PredictedHorizontalSpeed = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Drop",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="80.0"))
	float PredictedSpinSpeed = 28.0f;

	/** Масштаб скорости курсора, переводимой в скорость физической пригоршни. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Trajectory",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="3.0"))
	float TrajectoryVelocityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Trajectory",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="400.0"))
	float TrajectoryUpwardSpeed = 155.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Trajectory",
		meta=(ClampMin="10.0", UIMin="10.0", UIMax="800.0"))
	float TrajectoryMaximumSpeed = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Collisions",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DiceRestitution = 0.36f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Collisions",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float DiceFriction = 0.60f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Mouse Gather|Board",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="25.0"))
	float BoardWallInset = 6.5f;

	UPROPERTY(BlueprintAssignable, Category="Dice|Mouse Gather|Events")
	FMouseGatherStatusSignature OnStatusChanged;

	/** Передаёт фактические верхние грани единому scoring-контуру после завершения физики. */
	UPROPERTY(BlueprintAssignable, Category="Dice|Mouse Gather|Events")
	FMouseGatherFinishedSignature OnFinished;

private:
	struct FGuidedSettings
	{
		float UpwardSpeed = 0.0f;
		float HorizontalSpeed = 0.0f;
		float SpinSpeed = 0.0f;
		float AirLinearDamping = 0.0f;
		float AirAngularDamping = 0.0f;
		float FreeFlightTime = 0.0f;
		float AerialAlignmentLeadTime = 0.0f;
		float OrientationStrength = 0.0f;
		float OrientationDamping = 0.0f;
		float MaxAngularAcceleration = 0.0f;
		float LandingLinearDamping = 0.0f;
		float LandingAngularDamping = 0.0f;
		float RequiredStableTime = 0.0f;
		float MaximumRollTime = 0.0f;
		bool bAssistOnlyWhileFalling = false;
		bool bUseAirborneSafetyAlignment = false;
		bool bRetryWrongFaceWithBounce = false;
		bool bFreezeAfterLanding = true;
		TWeakObjectPtr<UPhysicalMaterial> PhysicalMaterial;
	};

	struct FGatheredDie
	{
		TWeakObjectPtr<ACPP_Dice> Dice;
		TWeakObjectPtr<UPrimitiveComponent> Body;
		TWeakObjectPtr<UDicePhysicsRollComponent> GuidedRoll;
		TWeakObjectPtr<USceneComponent> OriginalAttachParent;
		TWeakObjectPtr<UPhysicalMaterial> OriginalPhysicalMaterial;
		FName OriginalAttachSocket = NAME_None;
		float OriginalLinearDamping = 0.0f;
		float OriginalAngularDamping = 0.0f;
		ECollisionResponse OriginalWorldDynamicResponse = ECR_Block;
		bool bOriginalSimulatePhysics = false;
		bool bOriginalGravity = true;
		bool bOriginalNotifyRigidBodyCollision = false;
		bool bOriginalUseCCD = false;
		bool bOriginalCanRoll = true;
		bool bHadSupportContact = false;
		double LastImpactFeedbackTime = -1.0;
		float CarryPhase = 0.0f;
		FVector CarrySpinAxis = FVector::UpVector;
		int32 Result = 0;
		FGuidedSettings GuidedSettings;
	};

	UFUNCTION()
	void HandleNaturalDropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	void UpdateGathering(float DeltaSeconds);
	void UpdateNaturalDrop(float DeltaSeconds);
	void UpdatePredictedDrop();
	void TryGatherUnderCursor();
	bool GetCursorBoardPoint(FVector& OutPoint) const;
	FVector GetFormationOffset(int32 Index, int32 DiceCount) const;
	FVector GetFloatingOffset(const FGatheredDie& State) const;
	void FinishNaturalDrop();
	void FinishPredictedDrop();
	void RestoreGuidedSettings(FGatheredDie& State);
	void RestoreDie(FGatheredDie& State, bool bKeepFinalTransform);
	void PublishStatus(const FText& Status);
	void SetGeneratedNumber(AActor* DiceActor, int32 Result) const;
	FRotator GetLandingRotation(int32 Result) const;
	AActor* ResolveBoardActor();
	UPrimitiveComponent* ResolveBoardSurface();
	FVector GetBoardUpVector() const;
	float GetBoardClearance(const UPrimitiveComponent* Body) const;
	void SetBoardBoundaryInset(float EffectiveWallInset, UPrimitiveComponent* DiceBody);
	bool BeginGatherInternal(EMouseGatherDropMode NewMode, bool bUseTrajectoryGesture);
	UBoxComponent* CreateOrUpdateBoardCollider(FName ColliderName, const FVector& RelativeLocation,
		const FVector& LocalBoxExtent, ECollisionChannel DiceChannel);

	UPROPERTY(Transient)
	TObjectPtr<AActor> BoardActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BoardSurface = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> DropPhysicalMaterial = nullptr;

	TArray<FGatheredDie> GatheredDice;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACPP_Dice>> LastFinishedDice;

	EMouseGatherState InteractionState = EMouseGatherState::Idle;
	EMouseGatherDropMode DropMode = EMouseGatherDropMode::Natural;
	float DropElapsed = 0.0f;
	float StableElapsed = 0.0f;
	float CarryElapsed = 0.0f;
	FVector CarryAnchor = FVector::ZeroVector;
	bool bHasCarryAnchor = false;
	bool bWaitForActivationClickRelease = false;
	bool bPreviousLeftMouseDown = false;
	bool bUseTrajectoryGesture = false;
	bool bTrajectoryGestureActive = false;
	FVector PreviousGesturePoint = FVector::ZeroVector;
	FVector SmoothedGestureVelocity = FVector::ZeroVector;
	int32 LastNaturalDropImpactFeedbackCount = 0;

	static constexpr float BoardPlayableSurfaceInset = 2.8f;
	static constexpr float BoardWallHeight = 140.0f;
	static constexpr float BoardWallThickness = 12.0f;
	static constexpr float BoardFloorThickness = 12.0f;
};
