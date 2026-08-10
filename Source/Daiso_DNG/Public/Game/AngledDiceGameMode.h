// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AngledDiceGameMode.generated.h"

/**
 * Изолированный GameMode демонстрационной сцены: не создаёт BP_Player и тем самым
 * оставляет единственным действием пользователя кнопку броска в AAngledDiceHUD.
 */
UCLASS()
class DAISO_DNG_API AAngledDiceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Назначает минимальный HUD и отключает создание управляемого Pawn. */
	AAngledDiceGameMode();

	/**
	 * На время общего BeginPlay запрещает кубикам бросок, чтобы унаследованная
	 * Blueprint-связь Event BeginPlay -> RollDice не запускала автоматический стартовый бросок.
	 * Сразу после инициализации возвращает разрешение, поэтому UI-кнопка работает обычно.
	 */
	virtual void StartPlay() override;
};
