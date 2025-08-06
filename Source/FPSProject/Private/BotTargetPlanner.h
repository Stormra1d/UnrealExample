// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <PlayerAIController.h>
#include "BotTargetPlanner.generated.h"

class AFPSCharacter;
class AActor;

// This class does not need to be modified.
UCLASS()
class UBotTargetPlanner : public UObject
{
	GENERATED_BODY()

public:
	bool EvaluateBestTarget(APlayerAIController* Controller, AFPSCharacter* Player, AActor*& OutTarget);

private:
	AActor* FindNearestPickupOfType(UClass* PickupClass, const FVector& From);
	AActor* FindNearestEnemy(const FVector& From);
	AActor* FindRandomCollectible(const FVector& From);
};