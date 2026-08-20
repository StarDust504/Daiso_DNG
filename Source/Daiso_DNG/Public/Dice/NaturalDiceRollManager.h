// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaturalDiceRollManager.generated.h"

class ACPP_Dice;
class UBoxComponent;
class UDicePhysicsRollComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNaturalDiceRollStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNaturalDieSettledSignature, AActor*, DiceActor, int32, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNaturalDiceRollFinishedSignature, const TArray<int32>&, Results);

/**
 * Runs a batch of completely unguided Chaos dice throws.
 * No face is selected at launch: the result is read from the final physical rotation only after every die settles.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API ANaturalDiceRollManager : public AActor
{
	GENERATED_BODY()

public:
	ANaturalDiceRollManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Throws every playable dice actor in the world without generating a result in advance. */
	UFUNCTION(BlueprintCallable, Category="Dice|Natural Roll")
	bool RollAllDice();

	UFUNCTION(BlueprintPure, Category="Dice|Natural Roll")
	bool IsRolling() const { return bIsRolling; }

	/** Number of dice that visibly acquired upward velocity after a support impact in the previous roll. */
	UFUNCTION(BlueprintPure, Category="Dice|Natural Roll")
	int32 GetLastRollReboundCount() const { return LastRollReboundCount; }

	/** Number of board impacts which triggered the same sound/camera feedback as the guided roll. */
	UFUNCTION(BlueprintPure, Category="Dice|Natural Roll")
	int32 GetLastRollImpactFeedbackCount() const { return LastRollImpactFeedbackCount; }

	/** Reads the upward face for the standard SM_Dice_Basic orientation. Useful for tests and diagnostics. */
	static int32 DetermineTopFace(const FQuat& WorldRotation, const FVector& WorldUp = FVector::UpVector,
		float* OutAlignment = nullptr);

	/** The exact local face normals derived from the six landing rotations in the original BP_Dice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Faces")
	TMap<int32, FVector> FaceLocalNormals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="1000.0"))
	float UpwardSpeedMin = 455.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="1000.0"))
	float UpwardSpeedMax = 525.0f;

	/** Dice are sent through one loose convergence point, making mid-air and board collisions much more likely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="600.0"))
	float HorizontalSpeedMin = 145.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="600.0"))
	float HorizontalSpeedMax = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="80.0"))
	float SpinSpeedMin = 29.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="80.0"))
	float SpinSpeedMax = 43.0f;

	/** Random fraction blended into the direction toward the shared collision point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Throw",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirectionJitter = 0.28f;

	/** Restitution of the temporary dice material. About 0.34 gives a short, readable first rebound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Collisions",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DiceRestitution = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Collisions",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float DiceFriction = 0.62f;

	/** Small extra angular kick on real dice-to-dice contacts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Collisions",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
	float CollisionAngularKick = 3.25f;

	/** Moves the invisible wall faces onto the playable side of the visible wooden rim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Board",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="25.0"))
	float BoardWallInset = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Settle",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="100.0"))
	float SettleLinearSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Settle",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float SettleAngularSpeed = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Settle",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float RequiredGroupStableTime = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Settle",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="15.0"))
	float MaximumRollTime = 7.0f;

	/** Damping starts late, after the energetic collisions have already played out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Natural Roll|Settle",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float SettleDampingDelay = 2.35f;

	UPROPERTY(BlueprintAssignable, Category="Dice|Natural Roll|Events")
	FNaturalDiceRollStartedSignature OnNaturalRollStarted;

	UPROPERTY(BlueprintAssignable, Category="Dice|Natural Roll|Events")
	FNaturalDieSettledSignature OnNaturalDieSettled;

	UPROPERTY(BlueprintAssignable, Category="Dice|Natural Roll|Events")
	FNaturalDiceRollFinishedSignature OnNaturalRollFinished;

private:
	struct FActiveDie
	{
		TWeakObjectPtr<ACPP_Dice> Dice;
		TWeakObjectPtr<UPrimitiveComponent> Body;
		TWeakObjectPtr<UDicePhysicsRollComponent> FeedbackSource;
		TWeakObjectPtr<USceneComponent> OriginalAttachParent;
		TWeakObjectPtr<UPhysicalMaterial> OriginalPhysicalMaterial;
		FName OriginalAttachSocket = NAME_None;
		float OriginalLinearDamping = 0.0f;
		float OriginalAngularDamping = 0.0f;
		double LastCollisionKickTime = -1.0;
		double LastImpactFeedbackTime = -1.0;
		ECollisionResponse OriginalWorldDynamicResponse = ECR_Block;
		bool bOriginalSimulatePhysics = false;
		bool bOriginalGravity = true;
		bool bOriginalNotifyRigidBodyCollision = false;
		bool bOriginalUseCCD = false;
		bool bHasBeenAirborne = false;
		bool bHadSupportContact = false;
		bool bAwaitingRebound = false;
		bool bObservedRebound = false;
	};

	UFUNCTION()
	void HandleDiceHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	void StopActiveRoll(bool bPublishResults);
	void RestoreBody(FActiveDie& DieState);
	void SetGeneratedNumber(AActor* DiceActor, int32 Result) const;
	int32 DetermineTopFaceForBody(const UPrimitiveComponent* Body, float* OutAlignment = nullptr) const;
	FVector GetBoardUpVector() const;
	float GetBoardClearance(const UPrimitiveComponent* Body) const;
	bool RecoverEscapedDie(FActiveDie& DieState) const;
	AActor* ResolveBoardActor();
	UPrimitiveComponent* ResolveBoardSurface();
	void EnsureBoardBoundaries(UPrimitiveComponent* DiceBody, float EffectiveWallInset);
	UBoxComponent* CreateBoardCollider(FName ColliderName, const FVector& RelativeLocation,
		const FVector& LocalBoxExtent, ECollisionChannel DiceChannel);

	UPROPERTY(Transient)
	TObjectPtr<AActor> BoardActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BoardSurface = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> NaturalPhysicalMaterial = nullptr;

	TArray<FActiveDie> ActiveDice;
	bool bIsRolling = false;
	float RollElapsed = 0.0f;
	float GroupStableElapsed = 0.0f;
	int32 LastRollReboundCount = 0;
	int32 LastRollImpactFeedbackCount = 0;

	static constexpr float BoardPlayableSurfaceInset = 2.8f;
	static constexpr float BoardWallHeight = 140.0f;
	static constexpr float BoardWallThickness = 12.0f;
	static constexpr float BoardFloorThickness = 12.0f;
	static constexpr float MinimumAirborneClearance = 3.0f;
	static constexpr float EscapedDiceDepth = 18.0f;
};
