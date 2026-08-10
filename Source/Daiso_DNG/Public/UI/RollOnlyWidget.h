// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RollOnlyWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

/**
 * Минимальная нативная основа сцены броска. Blueprint сохраняет существующий
 * OnClicked-граф GenerateBTN, а этот класс отвечает только за русскую подпись кнопки.
 */
UCLASS(BlueprintType, Blueprintable)
class DAISO_DNG_API URollOnlyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Текст единственной интерактивной кнопки минимальной сцены. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Roll Only UI")
	FText RollButtonLabel = NSLOCTEXT("RollOnlyWidget", "RollButtonLabel", "Бросить кубики");

protected:
	/** Находит унаследованную GenerateBTN, оставляет её активной и заменяет видимую подпись. */
	virtual void NativeConstruct() override;

private:
	/**
	 * Создаёт кнопку в нативном WidgetTree, если дублированный Blueprint не содержит
	 * Designer-дерева. Это делает минимальную сцену независимой от основного HUD.
	 */
	void BuildFallbackRollButton();

	/**
	 * По нажатию вызывает у кубиков уже существующую Blueprint-функцию RollDice.
	 * Физика броска и сами кубики здесь не дублируются.
	 */
	UFUNCTION()
	void HandleRollButtonClicked();

	/** Рекурсивно ищет TextBlock внутри содержимого кнопки, не предполагая конкретную UMG-вложенность. */
	UTextBlock* FindFirstTextBlock(UWidget* RootWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RollButton = nullptr;
};
