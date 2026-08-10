// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "DicePhysicsRollComponent.generated.h"

class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;
class UBoxComponent;
class UCameraShakeBase;
class USoundAttenuation;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiceRollStartedSignature, int32, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDiceImpactSignature, float, Strength, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiceRollFinishedSignature, int32, Result);

/**
 * Добавляет управляемый физический бросок существующему Blueprint-актору кубика.
 * В начале кубик движется свободно, а при снижении компонент плавно направляет нужную грань вверх.
 * После первого настоящего контакта со столом компонент больше не поворачивает физическое тело.
 */
UCLASS(ClassGroup=(Dice), meta=(BlueprintSpawnableComponent, DisplayName="Dice Physics Roll"))
class DAISO_DNG_API UDicePhysicsRollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Создаёт компонент и стандартное соответствие значений локальным нормалям граней. */
	UDicePhysicsRollComponent();

	/** При старте игры заранее находит физическое тело кубика, если включён автоматический поиск. */
	virtual void BeginPlay() override;

	/** При завершении игры отменяет бросок и снимает подписки на столкновения. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Обновляет физическую симуляцию или финальное выравнивание текущего броска. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Запускает бросок к указанному значению, используя таблицу локальных нормалей граней. */
	UFUNCTION(BlueprintCallable, Category="Dice|Physics")
	bool RollToValue(int32 Result);

	/** Запускает бросок к явно заданному мировому повороту приземления. */
	UFUNCTION(BlueprintCallable, Category="Dice|Physics")
	bool RollToRotation(int32 Result, FRotator LandingRotation);

	/** Отменяет активный бросок и при необходимости полностью останавливает физическое тело. */
	UFUNCTION(BlueprintCallable, Category="Dice|Physics")
	void CancelRoll(bool bStopPhysics = true);

	/** Возвращает true, пока выполняется симуляция броска или финальное выравнивание. */
	UFUNCTION(BlueprintPure, Category="Dice|Physics")
	bool IsRolling() const;

	/** Optional explicit body. If empty, the first PrimitiveComponent on the owner is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Setup")
	FComponentReference DiceBodyReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Setup")
	bool bAutoFindDiceBody = true;

	/** Local outward normal of the numbered face. The requested face is pointed up, so its opposite lands on the table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Setup")
	TMap<int32, FVector> FaceLocalNormals;

	/** Optional physical material for bounce/friction tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Setup")
	TObjectPtr<UPhysicalMaterial> PhysicalMaterialOverride = nullptr;

	/** Optional explicit board instance. If empty, an actor tagged DiceBoard or whose class contains BP_Board is used. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Dice|Board Bounds")
	TObjectPtr<AActor> BoardActorOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	bool bAutoFindBoard = true;

	/** Preferred way to identify the board when a level contains more than one BP_Board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	FName BoardActorTag = TEXT("DiceBoard");

	/** Fallback class-name hint used by the current BP_Board_C. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	FString BoardClassNameHint = TEXT("BP_Board");

	/** Creates four shared invisible physical walls from the board mesh bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	bool bCreateBoardBoundaryWalls = true;

	/** Height above the board surface. Keep this above the maximum height of the dice centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="1.0", UIMin="1.0", UIMax="400.0"))
	float BoardWallHeight = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="1.0", UIMin="1.0", UIMax="100.0"))
	float BoardWallThickness = 12.0f;

	/** Thickness of the shared invisible floor placed directly below the board's top surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="1.0", UIMin="1.0", UIMax="100.0"))
	float BoardFloorThickness = 12.0f;

	/**
	 * Насколько игровая поверхность утоплена относительно самой высокой точки меша доски.
	 * Для текущей BP_Board это разница между верхом бортика и внутренним полем.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="0.0", UIMin="0.0", UIMax="50.0"))
	float BoardPlayableSurfaceInset = 2.8f;

	/** Включает CCD только на время броска, чтобы маленькое тело не проходило сквозь пол между physics-кадрами. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	bool bEnableContinuousCollisionDetection = true;

	/** Возвращает кубик над доской, если Chaos всё-таки пропустил защитный пол. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	bool bRecoverEscapedDice = true;

	/** Насколько ниже верхней плоскости доски должна оказаться нижняя точка кубика, чтобы считать его сбежавшим. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="1.0", UIMin="1.0", UIMax="100.0"))
	float EscapedDiceDepth = 18.0f;

	/** Малый вертикальный толчок после аварийного возврата, скрывающий телепорт за естественным отскоком. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="0.0", UIMin="0.0", UIMax="300.0"))
	float EscapeRecoveryUpwardSpeed = 90.0f;

	/** Positive values move the inside face of every wall further onto the board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="-100.0", ClampMax="100.0", UIMin="-30.0", UIMax="30.0"))
	float BoardWallInset = 0.0f;

	/** Keeps throws varied in the centre but steers them inward near an edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	bool bBiasThrowTowardBoardCenter = true;

	/** Constant amount of centre bias, even near the middle of the board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float BoardCenterBias = 0.12f;

	/** Additional bias at the very edge. 1 means an edge throw points almost directly inward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float BoardEdgeBias = 0.88f;

	/** Normalized distance from centre at which the stronger edge steering begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float BoardEdgeBiasStart = 0.52f;

	/** Aims all dice at a loose shared area around the board centre instead of launching each one in a random direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds")
	bool bClusterThrowsOnBoard = true;

	/** Radius of the landing area as a fraction of the board half-size. Lower values produce a tighter group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Board Bounds", meta=(ClampMin="0.05", ClampMax="1.0", UIMin="0.05", UIMax="1.0"))
	float BoardLandingClusterRadius = 0.38f;

	/** Initial upward velocity change, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw", meta=(ClampMin="0.0", UIMin="0.0", UIMax="1000.0"))
	float UpwardSpeed = 420.0f;

	/** Initial horizontal velocity change, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw", meta=(ClampMin="0.0", UIMin="0.0", UIMax="800.0"))
	float HorizontalSpeed = 95.0f;

	/** Initial tumbling speed, in radians per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw", meta=(ClampMin="0.0", UIMin="0.0", UIMax="50.0"))
	float SpinSpeed = 24.0f;

	/** Air damping kept deliberately low so the first half still looks uncontrolled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float AirLinearDamping = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float AirAngularDamping = 0.06f;

	/** Randomizes the final yaw, without changing which face is on top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw")
	bool bRandomizeLandingYaw = true;

	/** Aligns only the requested face and keeps the naturally acquired yaw, avoiding a visible twist on the table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Throw")
	bool bPreserveNaturalLandingYaw = true;

	/** Seconds of completely uncontrolled physics before the orientation assist may start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float FreeFlightTime = 0.16f;

	/** If true, the assist waits until the dice is no longer travelling upward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist")
	bool bAssistOnlyWhileFalling = false;

	/** Smooth fade-in for the aerial correction so it does not look like an instant snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.01", UIMin="0.01", UIMax="1.0"))
	float AssistRampTime = 0.12f;

	/** Physical face alignment starts only when the estimated board impact is this many seconds away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.1", UIMin="0.1", UIMax="1.5"))
	float AerialAlignmentLeadTime = 0.62f;

	/** Extra spring strength before the first board impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="1.0", UIMin="1.0", UIMax="4.0"))
	float AerialStrengthMultiplier = 2.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="1.0", UIMin="1.0", UIMax="4.0"))
	float AerialDampingMultiplier = 1.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="1.0", UIMin="1.0", UIMax="4.0"))
	float AerialAccelerationMultiplier = 2.20f;

	/**
	 * In the final part of the flight, use the remaining air time to close any small face error.
	 * The correction still happens before contact; it never rotates a die on the board.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist")
	bool bUseAirborneSafetyAlignment = true;

	/** Seconds before the predicted landing in which the airborne safety alignment may operate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.02", UIMin="0.02", UIMax="0.30"))
	float AirborneSafetyAlignmentTime = 0.10f;

	/** Errors smaller than this are left to physics, avoiding needless micro-corrections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float AirborneSafetyTolerance = 0.75f;

	/** Fraction of the natural yaw spin retained during the final airborne face capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float AirborneYawSpinRetention = 0.15f;

	/** Clearance above the board required before a later hit can count as a landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0"))
	float MinimumAirborneClearance = 3.0f;

	/** Minimum upward-facing contact normal for a hit to count as a landing rather than a wall strike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float LandingSurfaceNormalDot = 0.55f;

	/** Prevents any artificial orientation torque after the first real board impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist")
	bool bAllowPostImpactOrientationAssist = false;

	/** If the wrong face reaches the board, make a small physical hop and correct it during that new flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Correction Bounce")
	bool bRetryWrongFaceWithBounce = true;

	/** A landing farther from the target than this starts a correction bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Correction Bounce", meta=(ClampMin="1.0", ClampMax="89.0", UIMin="1.0", UIMax="89.0"))
	float CorrectionBounceTriggerAngle = 24.0f;

	/** Upward velocity of the corrective hop, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Correction Bounce", meta=(ClampMin="20.0", UIMin="20.0", UIMax="400.0"))
	float CorrectionBounceSpeed = 175.0f;

	/** Small random sideways kick so repeated hops do not look perfectly vertical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Correction Bounce", meta=(ClampMin="0.0", UIMin="0.0", UIMax="150.0"))
	float CorrectionBounceHorizontalSpeed = 14.0f;

	/** Directed angular kick toward the requested face, in radians per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Correction Bounce", meta=(ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
	float CorrectionBounceAngularSpeed = 5.0f;

	/** Safety limit. Normally one corrective hop is enough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Correction Bounce", meta=(ClampMin="0", ClampMax="5", UIMin="0", UIMax="5"))
	int32 MaximumCorrectionBounces = 2;

	/** Spring strength of the hidden rotational assist. Higher values find the face faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="150.0"))
	float OrientationStrength = 60.0f;

	/** Damping of the rotational assist. Raise this when the dice oscillates around the target face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="40.0"))
	float OrientationDamping = 8.0f;

	/** Safety cap for the assist, in rad/s^2. Lower values hide the correction better. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Landing Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="300.0"))
	float MaxAngularAcceleration = 140.0f;

	/** Damping used after the first meaningful table impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact", meta=(ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
	float LandingLinearDamping = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact", meta=(ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
	float LandingAngularDamping = 8.0f;

	/** Impacts below this speed do not fire the juice event or switch to landing damping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact", meta=(ClampMin="0.0", UIMin="0.0", UIMax="1000.0"))
	float MinimumImpactSpeed = 90.0f;

	/** Impact speed that maps to Strength=1 on OnDiceImpact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact", meta=(ClampMin="1.0", UIMin="1.0", UIMax="1500.0"))
	float FullStrengthImpactSpeed = 520.0f;

	/** Звук удара о доску. Оставьте пустым, пока нужный Sound Wave или Sound Cue не назначен в Details. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback")
	TObjectPtr<USoundBase> BoardImpactSound = nullptr;

	/** Общая громкость звука; фактическая громкость дополнительно зависит от силы удара. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Sound",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float BoardImpactSoundVolume = 0.75f;

	/** Минимальный случайный множитель высоты тона для естественного разнообразия ударов. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Sound",
		meta=(ClampMin="0.1", UIMin="0.8", UIMax="1.2"))
	float BoardImpactSoundPitchMin = 0.96f;

	/** Максимальный случайный множитель высоты тона для естественного разнообразия ударов. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Sound",
		meta=(ClampMin="0.1", UIMin="0.8", UIMax="1.2"))
	float BoardImpactSoundPitchMax = 1.04f;

	/** Необязательные настройки пространственного затухания звука. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Sound")
	TObjectPtr<USoundAttenuation> BoardImpactSoundAttenuation = nullptr;

	/** Включает короткую тряску камеры при ударе кубика о поверхность или границу доски. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Camera")
	bool bEnableBoardImpactCameraShake = true;

	/** Класс тряски камеры; по умолчанию используется встроенный лёгкий DiceImpactCameraShake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Camera")
	TSubclassOf<UCameraShakeBase> BoardImpactCameraShakeClass;

	/** Масштаб тряски; итоговая сила дополнительно зависит от скорости удара. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback|Camera",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float BoardImpactCameraShakeScale = 1.0f;

	/** Минимальный интервал между эффектами одного кубика, защищающий от дребезга контактов Chaos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Impact Feedback",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="0.5"))
	float ImpactFeedbackCooldown = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle", meta=(ClampMin="0.0", UIMin="0.0", UIMax="200.0"))
	float SettleLinearSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float SettleAngularSpeed = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle", meta=(ClampMin="0.0", UIMin="0.0", UIMax="45.0"))
	float SettleAngleTolerance = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle", meta=(ClampMin="0.0", UIMin="0.0", UIMax="2.0"))
	float RequiredStableTime = 0.18f;

	/** Guaranteed fallback if the dice gets wedged against scenery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle", meta=(ClampMin="0.1", UIMin="0.1", UIMax="10.0"))
	float MaximumRollTime = 3.25f;

	/** Duration of the small, normally invisible, final orientation correction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle", meta=(ClampMin="0.0", UIMin="0.0", UIMax="0.5"))
	float FinalAlignmentTime = 0.11f;

	/** Freeze and reattach the mesh after the result is final, so later bumps cannot change it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dice|Settle")
	bool bFreezeAfterLanding = true;

	UPROPERTY(BlueprintAssignable, Category="Dice|Events")
	FDiceRollStartedSignature OnDiceRollStarted;

	/** Strength is normalized to 0..1 and is intended for sound, camera shake, particles and hit-stop. */
	UPROPERTY(BlueprintAssignable, Category="Dice|Events")
	FDiceImpactSignature OnDiceImpact;

	UPROPERTY(BlueprintAssignable, Category="Dice|Events")
	FDiceRollFinishedSignature OnDiceRollFinished;

private:
	enum class ERollState : uint8
	{
		Idle,
		Simulating,
		FinalAlign
	};

	/** Принимает событие Chaos-столкновения и распознаёт удар, приземление или корректирующий отскок. */
	UFUNCTION()
	void HandleDiceHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	/** Возвращает явно назначенное либо первое подходящее физическое тело владельца. */
	UPrimitiveComponent* ResolveDiceBody();

	/** Сохраняет настройки тела, включает физику и прикладывает стартовые импульсы. */
	bool StartRoll(int32 Result, const FQuat& DesiredWorldRotation);

	/** Обновляет управляемый полёт и проверяет условия завершения броска. */
	void UpdateSimulation(float DeltaTime);

	/** Останавливает физику и запускает короткую доводку поворота до цели. */
	void BeginFinalAlignment();

	/** Плавно интерполирует поворот во время финального выравнивания. */
	void UpdateFinalAlignment(float DeltaTime);

	/** Завершает бросок, восстанавливает тело и рассылает итоговое значение. */
	void CompleteRoll(bool bApplyTargetRotation);

	/** Возвращает полную угловую ошибку относительно целевого поворота в радианах. */
	float GetOrientationErrorRadians() const;

	/** Возвращает отклонение выбранной грани от направления вверх в радианах. */
	float GetFaceUpErrorRadians() const;

	/** Оценивает оставшееся время до контакта с верхней плоскостью стола. */
	float GetEstimatedTimeToBoardImpact(const FVector& LinearVelocity) const;

	/** Возвращает зазор между нижней опорной точкой кубика и поверхностью стола. */
	float GetBoardClearance() const;

	/**
	 * Проверяет, не оказалось ли физическое тело целиком под защитным полом, и при необходимости
	 * возвращает его в ближайшую допустимую точку над доской. Возвращает true, если было выполнено восстановление.
	 */
	bool RecoverEscapedDice();

	/** Возвращает направление вверх для найденной поверхности стола. */
	FVector GetBoardUpVector() const;

	/** Восстанавливает исходное затухание тела и режим уведомлений о столкновениях. */
	void RestoreBodySettings();

	/** Воспроизводит назначенный звук и короткую тряску камеры с учётом силы удара. */
	void PlayBoardImpactFeedback(float Strength, const FVector& ImpactLocation);

	/** Возвращает явно заданный стол либо находит его по тегу и имени класса. */
	AActor* ResolveBoardActor();

	/** Выбирает наиболее крупный горизонтальный примитив найденного стола. */
	UPrimitiveComponent* ResolveBoardSurface();

	/** Создаёт общий невидимый пол и четыре ограничительные стенки игрового стола. */
	void EnsureBoardBoundaryWalls();

	/** Рассчитывает горизонтальную скорость броска с учётом положения на столе. */
	FVector GetBoardAwareHorizontalVelocity() const;

	/** Совмещает выбранную грань с верхом, сохраняя естественное рыскание кубика. */
	FQuat ResolveNaturalLandingYaw(const FVector& FaceNormalLocal, const FQuat& CurrentRotation) const;

	/** Создаёт и регистрирует один невидимый статический коллайдер границы стола. */
	UBoxComponent* CreateBoardCollider(FName ColliderName, const FVector& RelativeLocation, const FVector& LocalBoxExtent);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ActiveBody = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActiveBoardActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ActiveBoardSurface = nullptr;

	TWeakObjectPtr<USceneComponent> OriginalAttachParent;
	FName OriginalAttachSocket = NAME_None;
	FQuat TargetWorldRotation = FQuat::Identity;
	FQuat FinalAlignmentStartRotation = FQuat::Identity;
	FVector ActiveFaceNormalLocal = FVector::UpVector;
	ERollState RollState = ERollState::Idle;
	int32 ActiveResult = 0;
	float RollElapsed = 0.0f;
	float StableElapsed = 0.0f;
	float FinalAlignmentElapsed = 0.0f;
	float OriginalLinearDamping = 0.0f;
	float OriginalAngularDamping = 0.0f;
	bool bOriginalNotifyRigidBodyCollision = false;
	bool bOriginalUseCCD = false;
	bool bHasMeaningfulImpact = false;
	bool bHasBoardImpact = false;
	bool bHasBeenAirborne = false;
	bool bHasFaceTarget = false;
	bool bLandingYawResolved = true;
	int32 CorrectionBounceCount = 0;
	double LastImpactFeedbackTime = -1.0;
};
