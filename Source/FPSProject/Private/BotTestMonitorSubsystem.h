// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "../FPSCharacter.h"
#include "BotTestMonitorSubsystem.generated.h"

UENUM(BlueprintType)
enum class EBotTestOutcome : uint8 {
	None,
	Completed,
	Died,
	GotStuck,
	Timeout,
	Error
};

USTRUCT()
struct FBotTestRunData {
	GENERATED_BODY()

	UPROPERTY()
	FString RunName;

	UPROPERTY()
	EBotTestOutcome Outcome = EBotTestOutcome::None;

	UPROPERTY()
	float TimeTaken = 0.0f;

	UPROPERTY()
	float AvgFPS = 0.0f;

	UPROPERTY()
	float MaxMemoryMB = 0.0f;

	UPROPERTY()
	AFPSCharacter* CachedPlayer = nullptr;

	UPROPERTY()
	FString ReplayName;
};

USTRUCT()
struct FBotTestBatchResult {
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBotTestRunData> AllRuns;

	UPROPERTY()
	int32 CompletedCount = 0;

	UPROPERTY()
	int32 DiedCount = 0;

	UPROPERTY()
	int32 GotStuckCount = 0;

	UPROPERTY()
	int32 TimeoutCount = 0;

	UPROPERTY()
	int32 ErrorCount = 0;

	UPROPERTY()
	float AvgTime = 0.0f;

	UPROPERTY()
	float AvgFPS = 0.0f;

	UPROPERTY()
	float MaxMemoryPeak = 0.0f;
};

UCLASS()
class UBotTestMonitorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	EBotTestOutcome TestResult = EBotTestOutcome::None;
	bool bFinished = false;
	bool bReady = false;
	float TimeTaken = 0.0f;
	bool bIsBatchMode = false;
	FString ResultLogPath = TEXT("BotResults/Latest.json");

	float Elapsed = 0.0f;
	float MaxDuration = 500.0f;
	double StartSeconds = 0.0f;
	FString CurrentRunName;
	FString CurrentReplayName;
	FDateTime StartTimeStamp;

	void StartTest();
	void NotifyTestComplete(EBotTestOutcome Outcome, float TimeTakenParam);
	bool IsTestFinished() const { return bFinished; }

	void CollectPerformanceMetrics(float& OutAvgFPS, float& OutMaxMemory);

	FTSTicker::FDelegateHandle TickHandle;
	bool Tick(float DeltaTime);

	AFPSCharacter* TestPlayerPawn = nullptr;

	UFUNCTION(BlueprintCallable)
	void SetTestPlayerPawn(AFPSCharacter* InPawn)
	{
		TestPlayerPawn = InPawn;
	}

	AFPSCharacter* GetTestPlayerPawn() const { return TestPlayerPawn; }
	float GetMaxDuration() const { return MaxDuration; }

	bool bIsAIPlaytest = false;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void WriteSingleRunLog();
	void AppendToBatchLog();
	void StartReplay();
	void StopReplay();

	TWeakObjectPtr<UWorld> CurrentWorld;
	FString LastKnownMapName;
	float LevelTransitionTimer = 0.0f;
	static constexpr float MAX_LEVEL_TRANSITION_WAIT = 15.0f;

	float LastMovementTime = 0.f;
	FVector LastPos = FVector::ZeroVector;
	float MovementEps = 5.f;
	float SpeedEps = 5.f;
	float StuckTimeout = 60.f;

	void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);
	void OnWorldTearDown(UWorld* World);
};