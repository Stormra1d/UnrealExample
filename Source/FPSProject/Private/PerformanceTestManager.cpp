// Fill out your copyright notice in the Description page of Project Settings.


#include "PerformanceTestManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"


// Sets default values
APerformanceTestManager::APerformanceTestManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APerformanceTestManager::BeginPlay()
{
	Super::BeginPlay();
	CurrentCSVPath = FPaths::ProjectDir() + TEXT("PerformanceTestResults_Automation.csv");
	if (FPaths::FileExists(CurrentCSVPath)) {
		IFileManager::Get().Delete(*CurrentCSVPath);
	}

	CurrentTestIndex = 0;
	StartNextTest();
}

void APerformanceTestManager::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (bTestRunning) {
		TimeElapsed += DeltaTime;

		if (bIncrementalPeakHold) {
			IncrementalPeakTimeElapsed += DeltaTime;
			if (IncrementalPeakTimeElapsed >= HoldDuration) {
				bIncrementalPeakHold = false;
				EndTest();
			}
		} else if (TimeElapsed >= TestDuration) {
			EndTest();
		}
	}
}

void APerformanceTestManager::StartNextTest() {
	if (CurrentTestIndex >= TestSuite.Num()) {
		UE_LOG(LogTemp, Warning, TEXT("All tests complete"));
		return;
	}

	const FPerformanceTestConfig& Config = TestSuite[CurrentTestIndex];
	UE_LOG(LogTemp, Warning, TEXT("Starting Tests: %s"), *Config.Name);
	StartTest(Config);
}

void APerformanceTestManager::StartTest(const FPerformanceTestConfig& Config) {
	bTestRunning = true;
	TimeElapsed = 0.0f;

	CleanupActors();

	FPSSamples.Empty();

	switch (Config.TestType) {
	case EPerformanceTestType::Load:
		StartLoadTest(Config);
		break;
	case EPerformanceTestType::Stress:
		StartStressTest(Config);
		break;
	case EPerformanceTestType::Soak:
		StartSoakTest(Config);
		break;
	case EPerformanceTestType::Spike:
		StartSpikeTest(Config);
		break;
	}

	GetWorld()->GetTimerManager().SetTimer(LoggingTimerHandle, this, &APerformanceTestManager::LogPerformance, LoggingInterval, true);
}

void APerformanceTestManager::StartLoadTest(const FPerformanceTestConfig& Config) {
	switch (Config.LoadTestSubType) {
	case ELoadTestSubType::SteadyState:
		SpawnEnemies(Config.NumEnemies);
		SpawnPickups(Config.NumPickups);
		SpawnCollectibles(Config.NumCollectibles);
		TestDuration = Config.TestDuration;
		break;
	case ELoadTestSubType::Incremental:
		IncrementalEnemiesSpawned = 0;
		NumEnemiesThisTest = Config.NumEnemies;
		HoldDuration = Config.HoldDuration;
		bIncrementalPeakHold = false;
		IncrementalPeakTimeElapsed = 0.f;
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimerHandle, this, &APerformanceTestManager::IncrementalLoadTick, Config.SpawnInterval, true);
		break;
	}
}

void APerformanceTestManager::StartStressTest(const FPerformanceTestConfig& Config) {
	StressEnemiesSpawned = 0;
	NumEnemiesThisTest = Config.NumEnemies;
	StressSpawnBatchSize = 10;	
	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &APerformanceTestManager::StressRampTick, Config.SpawnInterval > 0 ? Config.SpawnInterval : 1.f, true
	);
	TestDuration = Config.TestDuration;
}

void APerformanceTestManager::StartSoakTest(const FPerformanceTestConfig& Config) {
	switch (Config.EnduranceTestSubType) {
	case EEnduranceTestSubType::General:
		SpawnEnemies(Config.NumEnemies);
		SpawnPickups(Config.NumPickups);
		TestDuration = Config.TestDuration;
		break;
	case EEnduranceTestSubType::MemoryLeak:
		SpawnEnemies(Config.NumEnemies);
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimerHandle, this, &APerformanceTestManager::LeakSpawnTick, Config.SpawnInterval > 0 ? Config.SpawnInterval : 1.f, true);
		break;
	case EEnduranceTestSubType::PerformanceDegredation:
		//TODO, nothing for now, it's the same as General
		break;
	}
}

void APerformanceTestManager::StartSpikeTest(const FPerformanceTestConfig& Config) {
	FTimerHandle TmpHandle;
	switch (Config.SpikeTestSubType) {
	case ESpikeTestSubType::Positive:
		SpawnEnemies(Config.SpikeSize);
		TestDuration = Config.TestDuration;
		break;
	case ESpikeTestSubType::Negative:
		SpawnEnemies(Config.SpikeSize);
		GetWorld()->GetTimerManager().SetTimer(TmpHandle, [this, Config]() {
			RemoveEnemies(Config.SpikeSize);
			}, 0.1f, false);
		TestDuration = Config.TestDuration;
		break;
	case ESpikeTestSubType::Repeated:
		SpikesCompleted = 0;
		CurrentNumSpikes = Config.NumSpikes;
		CurrentSpikeSize = Config.SpikeSize;
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimerHandle, this, &APerformanceTestManager::RepeatedSpikeTick, Config.SpikeInterval, true);
		TestDuration = Config.TestDuration;
		break;
	}
}

void APerformanceTestManager::IncrementalLoadTick() {
	if (IncrementalEnemiesSpawned < NumEnemiesThisTest) {
		SpawnEnemies(1);
		IncrementalEnemiesSpawned++;
	}
	else {
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
		bIncrementalPeakHold = true;
		IncrementalPeakTimeElapsed = 0.0f;
	}
}

void APerformanceTestManager::LeakSpawnTick() {
	SpawnEnemies(1);
}

void APerformanceTestManager::RepeatedSpikeTick() {
	if (SpikesCompleted < CurrentNumSpikes) {
		SpawnEnemies(CurrentSpikeSize);
		SpikesCompleted++;
	}
	else {
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void APerformanceTestManager::StressRampTick() {
	if (StressEnemiesSpawned < NumEnemiesThisTest) {
		int32 ToSpawn = FMath::Min(StressSpawnBatchSize, NumEnemiesThisTest - StressEnemiesSpawned);
		SpawnEnemies(ToSpawn);
		StressEnemiesSpawned += ToSpawn;
	}
	else {
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void APerformanceTestManager::EndTest() {
	bTestRunning = false;
	GetWorld()->GetTimerManager().ClearTimer(LoggingTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);

	LogPerformance();
	ExportResults();

	CleanupActors();

	CurrentTestIndex++;
	StartNextTest();
}

void APerformanceTestManager::SpawnEnemies(int32 Num) {
	if (!EnemyClass) return;

	for (int32 i = 0; i < Num; i++) {
		FVector SpawnLoc = GetRandomSpawnLocation();
		AActor* NewEnemy = GetWorld()->SpawnActor<AActor>(EnemyClass, SpawnLoc, FRotator::ZeroRotator);
		if (NewEnemy) {
			SpawnedEnemies.Add(NewEnemy);
		}
	}
}

void APerformanceTestManager::RemoveEnemies(int32 Num) {
	for (int32 i = 0; i < Num && SpawnedEnemies.Num() > 0; ++i) {
		if (IsValid(SpawnedEnemies[0])) {
			SpawnedEnemies[0]->Destroy();
		}
		SpawnedEnemies.RemoveAt(0);
	}
}

void APerformanceTestManager::SpawnPickups(int32 Num) {
	if (!PickupClass) return;

	for (int32 i = 0; i < Num; ++i) {
		FVector SpawnLoc = GetRandomSpawnLocation();
		AActor* NewPickup = GetWorld()->SpawnActor<AActor>(PickupClass, SpawnLoc, FRotator::ZeroRotator);
		if (NewPickup) {
			SpawnedPickups.Add(NewPickup);
		}
	}
}

void APerformanceTestManager::SpawnCollectibles(int32 Num) {
	if (!CollectibleClass) return;

	for (int32 i = 0; i < Num; ++i) {
		FVector SpawnLoc = GetRandomSpawnLocation();
		AActor* NewCollectible = GetWorld()->SpawnActor<AActor>(CollectibleClass, SpawnLoc, FRotator::ZeroRotator);
		if (NewCollectible) {
			SpawnedCollectibles.Add(NewCollectible);
		}
	}
}

void APerformanceTestManager::SpawnProjectiles(int32 Num) {
	if (!ProjectileClass) return;

	for (int32 i = 0; i < Num; ++i) {
		FVector SpawnLoc = GetRandomSpawnLocation();
		AActor* NewProjectile = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, FRotator::ZeroRotator);
		if (NewProjectile) {
			SpawnedProjectiles.Add(NewProjectile);
		}
	}
}

FVector APerformanceTestManager::GetRandomSpawnLocation() const {
	FVector Origin = GetActorLocation();

	float Angle = FMath::RandRange(0.f, 2 * PI);
	float Radius = FMath::RandRange(0.f, SpawnRadius);

	FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	FVector Location = Origin + Offset;
	Location.Z += 50.f;
	return Location;
}

void APerformanceTestManager::LogPerformance() {
	float FPS = 1.f / GetWorld()->GetDeltaSeconds();
	FPSSamples.Add(FPS);
	FString LogMsg = FString::Printf(TEXT("Time: %.1f | FPS: %.1f | Enemies: %d | Pickups: %d | Projectiles: %d | Collectibles: %d"),
		TimeElapsed, FPS, SpawnedEnemies.Num(), SpawnedPickups.Num(), SpawnedProjectiles.Num(), SpawnedCollectibles.Num());

	UE_LOG(LogTemp, Log, TEXT("%s"), *LogMsg);
}

void APerformanceTestManager::CleanupActors() {
	for (AActor* Enemy : SpawnedEnemies) {
		if (Enemy && IsValid(Enemy))
			Enemy->Destroy();
	}
	for (AActor* Pickup : SpawnedPickups) {
		if (Pickup && IsValid(Pickup))
			Pickup->Destroy();
	}
	for (AActor* Collectible : SpawnedCollectibles) {
		if (Collectible && IsValid(Collectible))
			Collectible->Destroy();
	}
	for (AActor* Projectile : SpawnedProjectiles) {
		if (Projectile && IsValid(Projectile))
			Projectile->Destroy();
	}

	SpawnedEnemies.Empty();
	SpawnedPickups.Empty();
	SpawnedCollectibles.Empty();
	SpawnedProjectiles.Empty();

	if (GetWorld()) {
		GetWorld()->GetTimerManager().ClearTimer(LoggingTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void APerformanceTestManager::ExportResults() {
	if (FPSSamples.Num() == 0) return;
	float MinFPS = FPSSamples[0];
	float MaxFPS = FPSSamples[0];
	float SumFPS = 0.f;
	for (float F : FPSSamples) {
		if (F < MinFPS) MinFPS = F;
		if (F > MaxFPS) MaxFPS = F;
		SumFPS += F;
	}
	float AvgFPS = SumFPS / FPSSamples.Num();

	FString CSVPath = CurrentCSVPath;
	if (!FPaths::FileExists(CSVPath)) {
		FString ConfigLine = FString::Printf(TEXT("#Tests: %d\n"), TestSuite.Num());
		FString CSVHeader = TEXT("Test,MinFPS,MaxFPS,AvgFPS,SampleCount\n");
		FFileHelper::SaveStringToFile(ConfigLine + CSVHeader, *CSVPath);
	}
	FString Row = FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%d\n"),
		*TestSuite[CurrentTestIndex].Name, MinFPS, MaxFPS, AvgFPS, FPSSamples.Num());

	FFileHelper::SaveStringToFile(Row, *CSVPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}