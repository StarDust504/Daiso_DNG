// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpectacleDiceRollManager.generated.h"

class ACPP_Dice;
class UBoxComponent;
class UDicePhysicsRollComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ESpectacleDiceMode : uint8
{
	None,
	VortexNatural,
	VortexPredicted,
	Meteors,
	GravityFlip,
	Handful,
	BackboardNatural,
	BackboardPredicted,
	BackboardDirected
};

UENUM(BlueprintType)
enum class ESpectacleDicePhase : uint8
{
	Idle,
	Vortex,
	MeteorDrop,
	GravityLift,
	HandfulAiming,
	BackboardAiming,
	NaturalSettling,
	PredictedSettling
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpectacleDiceStatusSignature, FText, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpectacleDiceFinishedSignature, const TArray<int32>&, Results);

/**
 * Owns the deliberately theatrical throws used only by the isolated dice-roll map.
 * The original dice Blueprints are left intact: their rigid bodies are borrowed, driven, and restored at runtime.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API ASpectacleDiceRollManager : public AActor
{
	GENERATED_BODY()

public:
	ASpectacleDiceRollManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Pulls all dice into a physical tornado, then releases them. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool StartVortex(bool bPredictedResult);

	/** Drops the dice one after another from different heights and positions. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool StartMeteors();

	/** Reverses gravity long enough to raise a tumbling cloud, then restores the fall. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool StartGravityFlip();

	/** Forms a compact handful under the cursor. Hold and release LMB to throw it. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool StartHandful();

	/** Automatically throws a staged group over the board edge farthest from the camera. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool StartBackboard(bool bPredictedResult);

	/** Stages the dice behind the board and waits for an LMB target on the playing surface. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool StartDirectedBackboard();

	/** Releases the directed backboard throw toward the board centre. Useful for Blueprint and tests. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool ReleaseDirectedBackboard();

	/** Releases an armed handful. Exposed for Blueprint variations and deterministic tests. */
	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	bool ReleaseHandful();

	UFUNCTION(BlueprintCallable, Category="Dice|Spectacle")
	void CancelSpecialRoll();

	UFUNCTION(BlueprintPure, Category="Dice|Spectacle")
	bool IsActive() const { return Phase != ESpectacleDicePhase::Idle; }

	UFUNCTION(BlueprintPure, Category="Dice|Spectacle")
	ESpectacleDiceMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category="Dice|Spectacle")
	ESpectacleDicePhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Dice|Spectacle")
	int32 GetActiveDiceCount() const { return ActiveDice.Num(); }

	/** Кубики последнего завершённого режима в том же порядке, что и результаты OnFinished. */
	TArray<ACPP_Dice*> GetLastFinishedDice() const;

	UFUNCTION(BlueprintPure, Category="Dice|Spectacle")
	int32 GetLastImpactFeedbackCount() const { return LastImpactFeedbackCount; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Vortex",
		meta=(ClampMin="0.5", UIMin="0.5", UIMax="6.0"))
	float VortexDuration = 2.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Vortex",
		meta=(ClampMin="5.0", UIMin="5.0", UIMax="100.0"))
	float VortexRadius = 31.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Vortex",
		meta=(ClampMin="10.0", UIMin="10.0", UIMax="150.0"))
	float VortexHeight = 54.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Vortex",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="10.0"))
	float VortexTurnsPerSecond = 1.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Meteors",
		meta=(ClampMin="40.0", UIMin="40.0", UIMax="300.0"))
	float MeteorHeight = 112.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Meteors",
		meta=(ClampMin="0.02", UIMin="0.02", UIMax="1.0"))
	float MeteorInterval = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Gravity Flip",
		meta=(ClampMin="0.5", UIMin="0.5", UIMax="5.0"))
	float GravityFlipDuration = 1.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Gravity Flip",
		meta=(ClampMin="10.0", UIMin="10.0", UIMax="150.0"))
	float GravityCloudHeight = 66.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Handful",
		meta=(ClampMin="10.0", UIMin="10.0", UIMax="150.0"))
	float HandfulHeight = 49.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Handful",
		meta=(ClampMin="2.0", UIMin="2.0", UIMax="20.0"))
	float HandfulSpacing = 6.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Handful",
		meta=(ClampMin="0.1", UIMin="0.1", UIMax="3.0"))
	float HandfulGestureScale = 0.82f;

	/** Distance outside the camera-far board edge used by the three backboard throws. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="5.0", UIMin="5.0", UIMax="100.0"))
	float BackboardOutsideDistance = 24.0f;

	/** Height of the staged dice centres above the playable surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="5.0", UIMin="5.0", UIMax="100.0"))
	float BackboardLaunchHeight = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="5.0", UIMin="5.0", UIMax="30.0"))
	float BackboardDiceSpacing = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="50.0", UIMin="50.0", UIMax="800.0"))
	float BackboardHorizontalSpeedMin = 330.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="50.0", UIMin="50.0", UIMax="800.0"))
	float BackboardHorizontalSpeedMax = 385.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="600.0"))
	float BackboardUpwardSpeedMin = 225.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Backboard",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="600.0"))
	float BackboardUpwardSpeedMax = 265.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Physics",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DiceRestitution = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Physics",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float DiceFriction = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Spectacle|Board",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="25.0"))
	float BoardWallInset = 6.5f;

	UPROPERTY(BlueprintAssignable, Category="Dice|Spectacle|Events")
	FSpectacleDiceStatusSignature OnStatusChanged;

	UPROPERTY(BlueprintAssignable, Category="Dice|Spectacle|Events")
	FSpectacleDiceFinishedSignature OnFinished;

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

	struct FActiveDie
	{
		TWeakObjectPtr<ACPP_Dice> Dice;
		TWeakObjectPtr<UPrimitiveComponent> Body;
		TWeakObjectPtr<UDicePhysicsRollComponent> GuidedRoll;
		TWeakObjectPtr<USceneComponent> OriginalAttachParent;
		TWeakObjectPtr<UPhysicalMaterial> OriginalPhysicalMaterial;
		FName OriginalAttachSocket = NAME_None;
		float OriginalLinearDamping = 0.0f;
		float OriginalAngularDamping = 0.0f;
		ECollisionEnabled::Type OriginalCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
		ECollisionResponse OriginalWorldDynamicResponse = ECR_Block;
		bool bOriginalSimulatePhysics = false;
		bool bOriginalGravity = true;
		bool bOriginalNotifyRigidBodyCollision = false;
		bool bOriginalUseCCD = false;
		bool bOriginalCanRoll = true;
		bool bHasBeenAirborne = false;
		bool bHadSupportContact = false;
		bool bMeteorLaunched = false;
		double LastImpactFeedbackTime = -1.0;
		double LastCollisionKickTime = -1.0;
		float PhaseOffset = 0.0f;
		FVector SpinAxis = FVector::UpVector;
		int32 Result = 0;
		FGuidedSettings GuidedSettings;
	};

	bool BeginMode(ESpectacleDiceMode NewMode);
	bool IsAnotherRollActive() const;
	void UpdateVortex(float DeltaSeconds);
	void ReleaseVortex();
	void UpdateMeteors(float DeltaSeconds);
	void UpdateGravityFlip(float DeltaSeconds);
	void UpdateHandful(float DeltaSeconds);
	void UpdateBackboardAiming(float DeltaSeconds);
	bool PrepareBackboardStaging();
	bool LaunchBackboardNatural(const FVector& Target);
	bool LaunchBackboardPredicted(const FVector& Target);
	bool ReleaseDirectedBackboardAt(const FVector& Target);
	bool ResolveBackboardFrame();
	FVector GetBackboardSpawnPoint(int32 Index) const;
	FVector GetAutomaticBackboardTarget() const;
	void OpenBackboardGate();
	void UpdateBackboardGate(float DeltaSeconds);
	void CloseBackboardGate();
	void ReleaseNaturalBodies(const FVector& SharedVelocity, float RandomHorizontalSpeed, float SpinSpeed);
	void ReleasePredictedVortex();
	void UpdateNaturalSettling(float DeltaSeconds);
	void UpdatePredictedSettling(float DeltaSeconds);
	void FinishNaturalRoll();
	void FinishPredictedRoll();
	void RestoreGuidedSettings(FActiveDie& State);
	void RestoreDie(FActiveDie& State);
	void PublishStatus(const FText& Status);
	void SetGeneratedNumber(AActor* DiceActor, int32 Result) const;
	FRotator GetLandingRotation(int32 Result) const;
	FVector GetCompactOffset(int32 Index, int32 Count, float Spacing) const;
	FVector GetBoardPoint(float NormalizedX, float NormalizedY, float Height) const;
	bool GetCursorBoardPoint(float Height, FVector& OutPoint) const;
	AActor* ResolveBoardActor();
	UPrimitiveComponent* ResolveBoardSurface();
	FVector GetBoardUpVector() const;
	float GetBoardClearance(const UPrimitiveComponent* Body) const;
	bool RecoverEscapedDie(FActiveDie& State) const;
	void SetBoardBoundaryInset(float EffectiveWallInset, UPrimitiveComponent* DiceBody);
	UBoxComponent* CreateOrUpdateBoardCollider(FName ColliderName, const FVector& RelativeLocation,
		const FVector& LocalBoxExtent, ECollisionChannel DiceChannel);

	UFUNCTION()
	void HandleNaturalHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(Transient)
	TObjectPtr<AActor> BoardActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BoardSurface = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> SpectaclePhysicalMaterial = nullptr;

	TArray<FActiveDie> ActiveDice;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACPP_Dice>> LastFinishedDice;

	ESpectacleDiceMode Mode = ESpectacleDiceMode::None;
	ESpectacleDicePhase Phase = ESpectacleDicePhase::Idle;
	float ModeElapsed = 0.0f;
	float PhaseElapsed = 0.0f;
	float StableElapsed = 0.0f;
	FVector EffectAnchor = FVector::ZeroVector;
	FVector PreviousHandfulAnchor = FVector::ZeroVector;
	FVector FilteredGestureVelocity = FVector::ZeroVector;
	FVector BackboardSpawnCenter = FVector::ZeroVector;
	FVector BackboardOutwardDirection = FVector::ForwardVector;
	FVector BackboardSideDirection = FVector::RightVector;
	FName BackboardGateName = NAME_None;
	int32 BackboardGateAxis = INDEX_NONE;
	float BackboardGateSign = 0.0f;
	float BackboardGateElapsed = 0.0f;
	bool bWaitForActivationClickRelease = false;
	bool bGestureDragging = false;
	bool bPreviousLeftMouseDown = false;
	bool bBackboardGateOpen = false;
	int32 LastImpactFeedbackCount = 0;

	static constexpr float BoardPlayableSurfaceInset = 2.8f;
	static constexpr float BoardWallHeight = 175.0f;
	static constexpr float BoardWallThickness = 12.0f;
	static constexpr float BoardFloorThickness = 12.0f;
	static constexpr float MinimumAirborneClearance = 3.0f;
	static constexpr float EscapedDiceDepth = 18.0f;
};
