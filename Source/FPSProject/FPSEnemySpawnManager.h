// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSEnemySpawnManager.generated.h"

UENUM()
enum class ESpawnerPhase : uint8 {
	Phase1,
	Phase2,
	Phase3
};

UCLASS()
class FPSPROJECT_API AFPSEnemySpawnManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AFPSEnemySpawnManager();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class AFPSEnemyPatrol> SmartEnemy;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class AFPSEnemyDumb> DumbEnemy;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	FVector SpawnOrigin = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, Category = "Spawner")
	float SpawnRadius = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	int32 MaxDumbEnemiesPhase1 = 4;

	ESpawnerPhase CurrentPhase = ESpawnerPhase::Phase1;
	int32 SmartKillCount = 0;

	AFPSEnemyPatrol* CurrentSmartEnemy = nullptr;
	TArray<AFPSEnemyDumb*> DumbEnemies;

	int32 Phase2Spawned = 0;
	float DumbEnemySpawnTimer = 0.0f;
	float Phase3SpawnTimer = 0.0f;

	FTimerHandle Phase2Handle;

protected:

	void SpawnSmartEnemy();
	void SpawnDumbEnemy();
	FVector GetRandomNavMeshPoint() const;
	void StartPhase2();

public:
	void NotifySmartEnemyDeath(AFPSEnemyPatrol* Enemy);
	void NotifyDumbEnemyDeath(AFPSEnemyDumb* Enemy);
};
