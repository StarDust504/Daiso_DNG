// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DicePhysicsRollComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

// Возвращает явно заданный стол либо находит его по тегу и резервной подсказке имени класса.
AActor* UDicePhysicsRollComponent::ResolveBoardActor()
{
	if (IsValid(BoardActorOverride))
	{
		ActiveBoardActor = BoardActorOverride;
		return ActiveBoardActor;
	}

	if (IsValid(ActiveBoardActor) || !bAutoFindBoard || !GetWorld())
	{
		return ActiveBoardActor;
	}

	AActor* NameHintCandidate = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == GetOwner())
		{
			continue;
		}

		if (!BoardActorTag.IsNone() && Candidate->ActorHasTag(BoardActorTag))
		{
			ActiveBoardActor = Candidate;
			return ActiveBoardActor;
		}

		if (!BoardClassNameHint.IsEmpty() && Candidate->GetClass()->GetName().Contains(BoardClassNameHint))
		{
			NameHintCandidate = Candidate;
		}
	}

	ActiveBoardActor = NameHintCandidate;
	return ActiveBoardActor;
}

// Выбирает у найденного стола примитив с наибольшей горизонтальной площадью как игровую поверхность.
UPrimitiveComponent* UDicePhysicsRollComponent::ResolveBoardSurface()
{
	if (IsValid(ActiveBoardSurface))
	{
		return ActiveBoardSurface;
	}

	AActor* Board = ResolveBoardActor();
	if (!IsValid(Board))
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Board->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	float LargestHorizontalArea = -1.0f;
	for (UPrimitiveComponent* Candidate : PrimitiveComponents)
	{
		if (!IsValid(Candidate) || Candidate->ComponentHasTag(TEXT("DiceBoundaryWall")))
		{
			continue;
		}

		const FBoxSphereBounds LocalBounds = Candidate->CalcBounds(FTransform::Identity);
		const float HorizontalArea = LocalBounds.BoxExtent.X * LocalBounds.BoxExtent.Y;
		if (HorizontalArea > LargestHorizontalArea)
		{
			LargestHorizontalArea = HorizontalArea;
			ActiveBoardSurface = Candidate;
		}
	}

	return ActiveBoardSurface;
}

// Один раз создаёт общий невидимый пол и четыре стенки по локальным границам поверхности стола.
void UDicePhysicsRollComponent::EnsureBoardBoundaryWalls()
{
	if (!bCreateBoardBoundaryWalls || !IsValid(ActiveBody))
	{
		return;
	}

	UPrimitiveComponent* BoardSurface = ResolveBoardSurface();
	if (!IsValid(BoardSurface) || !IsValid(ActiveBoardActor))
	{
		return;
	}

	const TArray<UActorComponent*> ExistingColliders = ActiveBoardActor->GetComponentsByTag(
		UBoxComponent::StaticClass(), TEXT("DiceBoundaryWall"));
	if (ExistingColliders.Num() >= 5)
	{
		return;
	}

	const FBoxSphereBounds LocalBounds = BoardSurface->CalcBounds(FTransform::Identity);
	const FVector LocalMin = LocalBounds.Origin - LocalBounds.BoxExtent;
	const FVector LocalMax = LocalBounds.Origin + LocalBounds.BoxExtent;
	const FVector AbsScale = BoardSurface->GetComponentTransform().GetScale3D().GetAbs()
		.ComponentMax(FVector(0.001f));
	const float HalfThicknessX = BoardWallThickness * 0.5f / AbsScale.X;
	const float HalfThicknessY = BoardWallThickness * 0.5f / AbsScale.Y;
	const float HalfFloorThickness = BoardFloorThickness * 0.5f / AbsScale.Z;
	const float HalfHeight = BoardWallHeight * 0.5f / AbsScale.Z;
	const float InsetX = BoardWallInset / AbsScale.X;
	const float InsetY = BoardWallInset / AbsScale.Y;
	const float TopZ = LocalMax.Z;
	const float WallZ = TopZ + HalfHeight;

	const float LeftX = LocalMin.X + InsetX - HalfThicknessX;
	const float RightX = LocalMax.X - InsetX + HalfThicknessX;
	const float BottomY = LocalMin.Y + InsetY - HalfThicknessY;
	const float TopY = LocalMax.Y - InsetY + HalfThicknessY;
	const float BoardHalfX = FMath::Max((LocalMax.X - LocalMin.X) * 0.5f, 1.0f);
	const float BoardHalfY = FMath::Max((LocalMax.Y - LocalMin.Y) * 0.5f, 1.0f);

	// Простой пол нужен, потому что Chaos иногда пропускает тонкую или сложную коллизию меша стола.
	CreateBoardCollider(TEXT("DiceBoundary_Floor"),
		FVector(LocalBounds.Origin.X, LocalBounds.Origin.Y, TopZ - HalfFloorThickness),
		FVector(BoardHalfX, BoardHalfY, HalfFloorThickness));
	CreateBoardCollider(TEXT("DiceBoundary_XMin"), FVector(LeftX, LocalBounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight));
	CreateBoardCollider(TEXT("DiceBoundary_XMax"), FVector(RightX, LocalBounds.Origin.Y, WallZ),
		FVector(HalfThicknessX, BoardHalfY + HalfThicknessY, HalfHeight));
	CreateBoardCollider(TEXT("DiceBoundary_YMin"), FVector(LocalBounds.Origin.X, BottomY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight));
	CreateBoardCollider(TEXT("DiceBoundary_YMax"), FVector(LocalBounds.Origin.X, TopY, WallZ),
		FVector(BoardHalfX + HalfThicknessX, HalfThicknessY, HalfHeight));
}

// Создаёт и регистрирует один невидимый статический коллайдер, блокирующий только канал кубика.
UBoxComponent* UDicePhysicsRollComponent::CreateBoardCollider(const FName ColliderName,
	const FVector& RelativeLocation, const FVector& LocalBoxExtent)
{
	if (!IsValid(ActiveBoardActor) || !IsValid(ActiveBoardSurface))
	{
		return nullptr;
	}

	if (UBoxComponent* ExistingCollider = FindObject<UBoxComponent>(ActiveBoardActor, *ColliderName.ToString()))
	{
		return ExistingCollider;
	}

	UBoxComponent* Wall = NewObject<UBoxComponent>(ActiveBoardActor, ColliderName);
	ActiveBoardActor->AddInstanceComponent(Wall);
	Wall->ComponentTags.AddUnique(TEXT("DiceBoundaryWall"));
	Wall->SetupAttachment(ActiveBoardSurface);
	Wall->SetRelativeLocation(RelativeLocation);
	Wall->SetRelativeRotation(FRotator::ZeroRotator);
	Wall->SetBoxExtent(LocalBoxExtent, false);
	Wall->SetMobility(ActiveBoardSurface->Mobility);
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
	Wall->SetCollisionResponseToChannel(ActiveBody->GetCollisionObjectType(), ECR_Block);
	Wall->SetGenerateOverlapEvents(false);
	Wall->SetCanEverAffectNavigation(false);
	Wall->SetHiddenInGame(true);
	Wall->RegisterComponent();
	return Wall;
}

// Рассчитывает горизонтальную скорость с учётом центра, краёв и зоны группировки на столе.
FVector UDicePhysicsRollComponent::GetBoardAwareHorizontalVelocity() const
{
	const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	FVector RandomDirection(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.0f);
	if (!IsValid(ActiveBoardSurface) || !IsValid(ActiveBody))
	{
		return RandomDirection * HorizontalSpeed;
	}

	const FTransform BoardTransform = ActiveBoardSurface->GetComponentTransform();
	const FBoxSphereBounds LocalBounds = ActiveBoardSurface->CalcBounds(FTransform::Identity);
	const FVector LocalPosition = BoardTransform.InverseTransformPosition(ActiveBody->GetComponentLocation());
	const FVector LocalOffset = LocalPosition - LocalBounds.Origin;

	if (bClusterThrowsOnBoard)
	{
		const float ClusterAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float ClusterRadius = FMath::Sqrt(FMath::FRand()) * BoardLandingClusterRadius;
		FVector TargetLocal = LocalBounds.Origin;
		TargetLocal.X += FMath::Cos(ClusterAngle) * LocalBounds.BoxExtent.X * ClusterRadius;
		TargetLocal.Y += FMath::Sin(ClusterAngle) * LocalBounds.BoxExtent.Y * ClusterRadius;
		TargetLocal.Z = LocalBounds.Origin.Z + LocalBounds.BoxExtent.Z;

		const FVector TargetWorld = BoardTransform.TransformPosition(TargetLocal);
		FVector ToTarget = TargetWorld - ActiveBody->GetComponentLocation();
		ToTarget.Z = 0.0f;
		const float GravityMagnitude = FMath::Max(
			FMath::Abs(GetWorld() ? GetWorld()->GetGravityZ() : -980.0f), 1.0f);
		const float HeightAboveBoard = FMath::Max(
			ActiveBody->GetComponentLocation().Z - TargetWorld.Z, 0.0f);
		const float ExpectedAirTime = (UpwardSpeed + FMath::Sqrt(
			FMath::Square(UpwardSpeed) + 2.0f * GravityMagnitude * HeightAboveBoard)) / GravityMagnitude;
		return (ToTarget / FMath::Max(ExpectedAirTime, 0.20f)).GetClampedToMaxSize(HorizontalSpeed);
	}

	if (!bBiasThrowTowardBoardCenter)
	{
		return RandomDirection * HorizontalSpeed;
	}

	const float NormalizedX = FMath::Abs(LocalOffset.X) / FMath::Max(LocalBounds.BoxExtent.X, 1.0f);
	const float NormalizedY = FMath::Abs(LocalOffset.Y) / FMath::Max(LocalBounds.BoxExtent.Y, 1.0f);
	const float EdgeAmount = FMath::GetRangePct(
		BoardEdgeBiasStart, 1.0f, FMath::Max(NormalizedX, NormalizedY));
	const float Bias = FMath::Clamp(FMath::Lerp(
		BoardCenterBias, BoardEdgeBias, FMath::Clamp(EdgeAmount, 0.0f, 1.0f)), 0.0f, 1.0f);

	FVector ToCenter = BoardTransform.TransformVectorNoScale(FVector(-LocalOffset.X, -LocalOffset.Y, 0.0f));
	ToCenter.Z = 0.0f;
	ToCenter = ToCenter.GetSafeNormal();
	if (ToCenter.IsNearlyZero())
	{
		return RandomDirection * HorizontalSpeed;
	}

	RandomDirection = FMath::Lerp(RandomDirection, ToCenter, Bias).GetSafeNormal();
	return (RandomDirection.IsNearlyZero() ? ToCenter : RandomDirection) * HorizontalSpeed;
}

// Совмещает нужную грань с верхом, сохраняя естественное рыскание текущего полёта.
FQuat UDicePhysicsRollComponent::ResolveNaturalLandingYaw(const FVector& FaceNormalLocal,
	const FQuat& CurrentRotation) const
{
	const FVector Normal = FaceNormalLocal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return TargetWorldRotation;
	}

	const FQuat FaceToUp = FQuat::FindBetweenNormals(Normal, FVector::UpVector);
	const FVector ReferenceAxis = FMath::Abs(FVector::DotProduct(Normal, FVector::ForwardVector)) < 0.9f
		? FVector::ForwardVector
		: FVector::RightVector;
	const FVector LocalTangent = (ReferenceAxis - Normal * FVector::DotProduct(ReferenceAxis, Normal))
		.GetSafeNormal();
	FVector BaseTangent = FaceToUp.RotateVector(LocalTangent);
	FVector CurrentTangent = CurrentRotation.RotateVector(LocalTangent);
	BaseTangent.Z = 0.0f;
	CurrentTangent.Z = 0.0f;
	BaseTangent = BaseTangent.GetSafeNormal();
	CurrentTangent = CurrentTangent.GetSafeNormal();
	if (BaseTangent.IsNearlyZero() || CurrentTangent.IsNearlyZero())
	{
		return FaceToUp;
	}

	const float YawAngle = FMath::Atan2(FVector::CrossProduct(BaseTangent, CurrentTangent).Z,
		FVector::DotProduct(BaseTangent, CurrentTangent));
	return (FQuat(FVector::UpVector, YawAngle) * FaceToUp).GetNormalized();
}

// Возвращает расстояние между нижней опорной точкой кубика и верхней плоскостью стола.
float UDicePhysicsRollComponent::GetBoardClearance() const
{
	if (!IsValid(ActiveBody) || !IsValid(ActiveBoardSurface))
	{
		return -1.0f;
	}

	const FTransform BoardTransform = ActiveBoardSurface->GetComponentTransform();
	const FBoxSphereBounds LocalBounds = ActiveBoardSurface->CalcBounds(FTransform::Identity);
	const FVector BoardTop = BoardTransform.TransformPosition(
		FVector(LocalBounds.Origin.X, LocalBounds.Origin.Y, LocalBounds.Origin.Z + LocalBounds.BoxExtent.Z));
	const FVector BoardUp = GetBoardUpVector();
	const float BodySupportRadius = FVector::DotProduct(
		ActiveBody->Bounds.BoxExtent.GetAbs(), BoardUp.GetAbs());
	return FVector::DotProduct(ActiveBody->GetComponentLocation() - BoardTop, BoardUp) - BodySupportRadius;
}

// Возвращает нормализованное направление вверх для текущей поверхности стола.
FVector UDicePhysicsRollComponent::GetBoardUpVector() const
{
	return IsValid(ActiveBoardSurface)
		? ActiveBoardSurface->GetComponentTransform()
			.TransformVectorNoScale(FVector::UpVector).GetSafeNormal()
		: FVector::UpVector;
}
