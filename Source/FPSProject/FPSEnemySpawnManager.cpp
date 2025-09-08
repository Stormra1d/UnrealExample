// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSEnemySpawnManager.h"
#include "FPSEnemyPatrol.h"
#include "FPSEnemyDumb.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

// Sets default values
AFPSEnemySpawnManager::AFPSEnemySpawnManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFPSEnemySpawnManager::BeginPlay()
{
	Super::BeginPlay();
	SpawnSmartEnemy();
	
}

// Called every frame
void AFPSEnemySpawnManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentPhase == ESpawnerPhase::Phase1) {
		DumbEnemies.RemoveAll([](AFPSEnemyDumb* E) {
			return !IsValid(E);
			});

		DumbEnemySpawnTimer += DeltaTime;
		int32 AvailableSlots = MaxDumbEnemiesPhase1 - DumbEnemies.Num();
		if (DumbEnemySpawnTimer >= 10.0f && AvailableSlots > 0) {
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Spawning Dumb enemy"));
			SpawnDumbEnemy();
			DumbEnemySpawnTimer = 0.0f;
		}
	}
	else if (CurrentPhase == ESpawnerPhase::Phase2) {
		DumbEnemies.RemoveAll([](AFPSEnemyDumb* E) {
			return !IsValid(E);
			});
		if (Phase2Spawned <= 0 && DumbEnemies.Num() == 0) {
			CurrentPhase = ESpawnerPhase::Phase3;
			Phase3SpawnTimer = 0.0f;
		}
	}
	else if (CurrentPhase == ESpawnerPhase::Phase3) {
		Phase3SpawnTimer += DeltaTime;
		if (Phase3SpawnTimer >= 5.0f) {
			if (FMath::RandBool()) {
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Spawning Dumb enemy, P3"));
				SpawnDumbEnemy();
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Spawning Smart enemy, P3"));
				SpawnSmartEnemy();
			}

			Phase3SpawnTimer = 0.0f;
		}
		DumbEnemies.RemoveAll([](AFPSEnemyDumb* E) {
			return !IsValid(E);
			});
	}
}

void AFPSEnemySpawnManager::SpawnSmartEnemy() {
	if (!SmartEnemy) {
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("SMART ENEMY NOT SET!"));
		CurrentSmartEnemy = nullptr;
		return;
	}
	FVector Loc = GetRandomNavMeshPoint();
	FRotator Rot = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFPSEnemyPatrol* Enemy = GetWorld()->SpawnActor<AFPSEnemyPatrol>(SmartEnemy, Loc, Rot, SpawnParams);
	if (Enemy) {
		Enemy->SpawnDefaultController();
		CurrentSmartEnemy = Enemy;
		Enemy->OwningSpawner = this;
	} else {
		CurrentSmartEnemy = nullptr;
	}
}

void AFPSEnemySpawnManager::SpawnDumbEnemy() {
	if (!DumbEnemy) return;
	FVector Loc = GetRandomNavMeshPoint();
	FRotator Rot = FRotator::ZeroRotator;
	Loc.Z += 100.0f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFPSEnemyDumb* Enemy = GetWorld()->SpawnActor<AFPSEnemyDumb>(DumbEnemy, Loc, Rot, SpawnParams);
	if (Enemy) {
		Enemy->SpawnDefaultController();
		DumbEnemies.Add(Enemy);
		Enemy->OwningSpawner = this;
		if (CurrentPhase == ESpawnerPhase::Phase2) {
			Phase2Spawned++;
		}
	}
}

FVector AFPSEnemySpawnManager::GetRandomNavMeshPoint() const {
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) {
		FVector RandomOffset = FVector(FMath::RandRange(-SpawnRadius, SpawnRadius), FMath::RandRange(-SpawnRadius, SpawnRadius), 0.0f);
		return SpawnOrigin + RandomOffset;
	}
	FNavLocation NavLoc;
	if (NavSys->GetRandomReachablePointInRadius(SpawnOrigin, SpawnRadius, NavLoc)) return NavLoc.Location;
	FVector RandomOffset = FVector(FMath::RandRange(-SpawnRadius * 0.5f, SpawnRadius * 0.5f), FMath::RandRange(-SpawnRadius * 0.5f, SpawnRadius * 0.5f), 0.0f);
	return SpawnOrigin + RandomOffset;
}

void AFPSEnemySpawnManager::StartPhase2() {
	for (int i = 0; i < 5; i++) {
		SpawnDumbEnemy();
	}
	Phase2Spawned = 5;

	GetWorldTimerManager().SetTimer(Phase2Handle, [this]() {
		for (int i = 0; i < 5; i++) {
			SpawnDumbEnemy();
		}
		Phase2Spawned += 5;
		}, 10.0f, false);
}

void AFPSEnemySpawnManager::NotifySmartEnemyDeath(AFPSEnemyPatrol* Enemy) {
	if (CurrentSmartEnemy == Enemy) {
		SmartKillCount++;
		CurrentSmartEnemy = nullptr;
		if (CurrentPhase == ESpawnerPhase::Phase1) {
			if (SmartKillCount < 3) {
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Spawning Smart enemy, P1"));
				SpawnSmartEnemy();
			}
			else {
				CurrentPhase = ESpawnerPhase::Phase2;
				StartPhase2();
			}
		}
	}
}

void AFPSEnemySpawnManager::NotifyDumbEnemyDeath(AFPSEnemyDumb* Enemy) {
	DumbEnemies.Remove(Enemy);
	if (CurrentPhase == ESpawnerPhase::Phase2)
		Phase2Spawned--;
}
