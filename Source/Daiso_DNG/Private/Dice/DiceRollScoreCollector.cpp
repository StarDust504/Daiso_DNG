// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice/DiceRollScoreCollector.h"

#include "Dice/DicePhysicsRollComponent.h"
#include "Dice/DiceScoringLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

// Создаёт коллектор без Tick и назначает стандартную таблицу правил подсчёта.
ADiceRollScoreCollector::ADiceRollScoreCollector()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultRules(
		TEXT("/Game/Data/DT_DiceScoringRules.DT_DiceScoringRules"));
	if (DefaultRules.Succeeded())
	{
		ScoringRules = DefaultRules.Object;
	}
}

// При запуске очищает промежуточный бросок и подключается к шести кубикам уровня.
void ADiceRollScoreCollector::BeginPlay()
{
	Super::BeginPlay();
	ResetCollectedRoll();
	ConnectToDice();
}

// Перед уничтожением актора снимает динамические подписки со всех кубиков.
void ADiceRollScoreCollector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindDice();
	Super::EndPlay(EndPlayReason);
}

// Находит ровно шесть компонентов броска, сортирует их для стабильности и подписывается на результаты.
bool ADiceRollScoreCollector::ConnectToDice()
{
	UnbindDice();

	TArray<UDicePhysicsRollComponent*> Candidates;
	if (!DiceActors.IsEmpty())
	{
		for (AActor* DiceActor : DiceActors)
		{
			if (IsValid(DiceActor))
			{
				if (UDicePhysicsRollComponent* Component = DiceActor->FindComponentByClass<UDicePhysicsRollComponent>())
				{
					Candidates.AddUnique(Component);
				}
			}
		}
	}
	else if (bAutoFindDiceActors && IsValid(GetWorld()))
	{
		for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
		{
			if (UDicePhysicsRollComponent* Component = ActorIt->FindComponentByClass<UDicePhysicsRollComponent>())
			{
				Candidates.Add(Component);
			}
		}
	}

	Candidates.Sort([](const UDicePhysicsRollComponent& A, const UDicePhysicsRollComponent& B)
	{
		const AActor* OwnerA = A.GetOwner();
		const AActor* OwnerB = B.GetOwner();
		return IsValid(OwnerA) && IsValid(OwnerB)
			? OwnerA->GetFName().LexicalLess(OwnerB->GetFName())
			: OwnerA != nullptr;
	});

	if (Candidates.Num() != DicePerRoll)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s expected exactly 6 dice with DicePhysicsRoll components, but found %d."),
			*GetName(), Candidates.Num());
		return false;
	}

	for (UDicePhysicsRollComponent* Component : Candidates)
	{
		Component->OnDiceRollFinished.AddUniqueDynamic(this, &ADiceRollScoreCollector::HandleDiceRollFinished);
		BoundDiceComponents.Add(Component);
	}
	UE_LOG(LogTemp, Display, TEXT("%s connected to 6 dice and is ready to score."), *GetName());
	return true;
}

// Очищает только накапливаемые значения, сохраняя последний опубликованный результат.
void ADiceRollScoreCollector::ResetCollectedRoll()
{
	CurrentRollResults.Reset(DicePerRoll);
}

// Проверяет одно значение и после шестого кубика автоматически публикует полный результат.
bool ADiceRollScoreCollector::SubmitDieResult(const int32 Result)
{
	if (Result < 1 || Result > 6)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected invalid die result %d."), *GetName(), Result);
		return false;
	}

	CurrentRollResults.Add(Result);
	if (CurrentRollResults.Num() == DicePerRoll)
	{
		PublishScore();
		ResetCollectedRoll();
	}
	return true;
}

// Возвращает количество компонентов, от которых сейчас принимаются результаты.
int32 ADiceRollScoreCollector::GetConnectedDiceCount() const
{
	return BoundDiceComponents.Num();
}

// Передаёт результат физически остановившегося кубика в общий накопитель.
void ADiceRollScoreCollector::HandleDiceRollFinished(const int32 Result)
{
	SubmitDieResult(Result);
}

// Снимает подписки со всех подключённых компонентов и очищает их список.
void ADiceRollScoreCollector::UnbindDice()
{
	for (UDicePhysicsRollComponent* Component : BoundDiceComponents)
	{
		if (IsValid(Component))
		{
			Component->OnDiceRollFinished.RemoveDynamic(this, &ADiceRollScoreCollector::HandleDiceRollFinished);
		}
	}
	BoundDiceComponents.Reset();
}

// Рассчитывает шесть накопленных значений, рассылает событие и выводит отладочный итог.
void ADiceRollScoreCollector::PublishScore()
{
	LastScoreResult = UDiceScoringLibrary::CalculateDiceRollScore(CurrentRollResults, ScoringRules);
	OnRollScored.Broadcast(LastScoreResult);

	const FString Message = LastScoreResult.bIsValid
		? FString::Printf(TEXT("Dice score: %d | combinations: %d | bust: %s"),
			LastScoreResult.TotalScore,
			LastScoreResult.Combinations.Num(),
			LastScoreResult.bIsBust ? TEXT("YES") : TEXT("NO"))
		: FString::Printf(TEXT("Dice scoring error: %s"), *LastScoreResult.ErrorMessage);

	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	if (bPrintScoreToScreen && GEngine)
	{
		const FColor Color = LastScoreResult.bIsValid
			? (LastScoreResult.bIsBust ? FColor::Yellow : FColor::Green)
			: FColor::Red;
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, Color, Message);
	}
}
