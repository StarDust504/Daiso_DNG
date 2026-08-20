// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDicePhysicsRollComponent;
class UPrimitiveComponent;
struct FHitResult;

/**
 * Replays the impact feedback configured on an existing guided-roll component.
 * Natural modes use this bridge without taking over the component's roll simulation.
 */
namespace DiceImpactFeedbackBridge
{
	DAISO_DNG_API bool PlayMatchingGuidedImpact(
		UDicePhysicsRollComponent* FeedbackSource,
		UPrimitiveComponent* HitBody,
		const FVector& NormalImpulse,
		const FHitResult& Hit,
		double& InOutLastFeedbackTime);
}
