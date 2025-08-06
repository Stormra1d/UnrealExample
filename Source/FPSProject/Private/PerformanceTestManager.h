// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerformanceTestManager.generated.h"

UENUM(BlueprintType)
enum class EPerformanceTestType : uint8 {
	Load, Stress, Soak, Spike
};

UENUM(BlueprintType)
enum class ELoadTestSubType : uint8 {
	SteadyState, Incremental
};

UENUM(BlueprintType)
enum class EEnduranceTestSubType : uint8 {
	General, MemoryLeak, PerformanceDegredation
};


UENUM(BlueprintType)
enum class ESpikeTestSubType : uint8 {
	Positive, Negative, Repeated
};

USTRUCT(BlueprintType)
struct FPerformanceTestConfig {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPerformanceTestType TestType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TestType == EPerformanceTestType::Load"))
	ELoadTestSubType LoadTestSubType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TestType == EPerformanceTestType::Soak"))
	EEnduranceTestSubType EnduranceTestSubType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TestType == EPerformanceTestType::Spike"))
	ESpikeTestSubType SpikeTestSubType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumEnemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumPickups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumCollectibles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumProjectiles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TestType == EPerformanceTestType::Spike"))
	int32 SpikeSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TestType == EPerformanceTestType::Spike"))
	int32 NumSpikes; 
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TestType == EPerformanceTestType::Spike"))
	float SpikeInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HoldDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TestDuration = 60.0f;
};


UCLASS()
class APerformanceTestManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APerformanceTestManager();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerformanceTest")
	TArray<FPerformanceTestConfig> TestSuite;

	UPROPERTY(EditAnywhere, Category = "PerformanceTest")
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditAnywhere, Category = "PerformanceTest")
	TSubclassOf<AActor> PickupClass;

	UPROPERTY(EditAnywhere, Category = "PerformanceTest")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "PerformanceTest")
	TSubclassOf<AActor> CollectibleClass;

	UPROPERTY(EditAnywhere, Category = "PerformanceTest")
	float SpawnRadius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "PerformanceTest")
	float LoggingInterval = 1.0f;

	int32 IncrementalEnemiesSpawned;

	TArray<AActor*> SpawnedEnemies;
	TArray<AActor*> SpawnedPickups;
	TArray<AActor*> SpawnedProjectiles;
	TArray<AActor*> SpawnedCollectibles;

	int32 CurrentTestIndex = 0;
	float TestDuration;
	float TimeElapsed = 0.0f;
	FTimerHandle LoggingTimerHandle;
	FTimerHandle SpawnTimerHandle;
	bool bTestRunning = false;

	void StartNextTest();
	void StartTest(const FPerformanceTestConfig& Config);
	void EndTest();

	void StartLoadTest(const FPerformanceTestConfig& Config);
	void StartStressTest(const FPerformanceTestConfig& Config);
	void StartSoakTest(const FPerformanceTestConfig& Config);
	void StartSpikeTest(const FPerformanceTestConfig& Config);

	void IncrementalLoadTick();
	void LeakSpawnTick();
	void RepeatedSpikeTick();
	void StressRampTick();

	void SpawnEnemies(int32 Num);
	void RemoveEnemies(int32 Num);
	void SpawnPickups(int32 Num);
	void SpawnCollectibles(int32 Num);
	void SpawnProjectiles(int32 Num);

	FVector GetRandomSpawnLocation() const;
	void LogPerformance();
	void CleanupActors();

	TArray<float> FPSSamples;
	void ExportResults();

private:
	int32 NumEnemiesThisTest;
	int32 SpikesCompleted;
	int32 CurrentNumSpikes;
	int32 CurrentSpikeSize;
	float IncrementalPeakTimeElapsed;
	bool bIncrementalPeakHold = false;
	int32 StressEnemiesSpawned;
	FTimerHandle PeakHoldTimerHandle;
	float HoldDuration;
	int32 StressSpawnBatchSize = 10;

	FString CurrentCSVPath;
};
