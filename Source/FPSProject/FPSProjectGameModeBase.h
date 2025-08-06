// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include <BotTargetPlanner.h>
#include <PlayerAIController.h>
#include "FPSProjectGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AFPSProjectGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

	virtual void StartPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnablePlayerAI = false;

	UPROPERTY()
	UBotTargetPlanner* BotTargetPlanner;

	FTimerHandle TargetUpdateTimerHandle;

	UPROPERTY()
	APlayerAIController* CachedAIController = nullptr;

	UPROPERTY()
	class AFPSCharacter* CachedPlayerCharacter;

	void UpdateBotTarget();

	void SetupPlayerAI();
};
