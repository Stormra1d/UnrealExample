// Fill out your copyright notice in the Description page of Project Settings.

#include "FPSPickupSpawner.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "CollectiblePickup.h"
#include "HealthPackPickup.h"
#include "AmmoCratePickup.h"

// Sets default values
AFPSPickupSpawner::AFPSPickupSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFPSPickupSpawner::BeginPlay()
{
	Super::BeginPlay();
	SpawnCollectibles();
	HealthPackSpawnTimer = 20.0f;
	
}

// Called every frame
void AFPSPickupSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	HealthPackSpawnTimer += DeltaTime;
	AmmoCrateSpawnTimer += DeltaTime;

	if (HealthPackSpawnTimer >= RespawnTime) {
		TryRespawnHealthPack();
		HealthPackSpawnTimer = 0.0f;
	}

	if (AmmoCrateSpawnTimer >= RespawnTime) {
		TryRespawnAmmoCrate();
		AmmoCrateSpawnTimer = 0.0f;
	}
}

void AFPSPickupSpawner::SpawnCollectibles() {
	for (int32 i = 0; i < NumberRubies; i++) {
		FVector Loc = GetRandomNavmeshLocation();
		Loc.Z += 50.0f;
		if (RedRubyClass) {
			ACollectiblePickup* NewPickup = GetWorld()->SpawnActor<ACollectiblePickup>(RedRubyClass, Loc, FRotator::ZeroRotator);
			if (NewPickup) {
				NewPickup->SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
			}
		}
	}

	for (int32 i = 0; i < NumberSapphires; i++) {
		FVector Loc = GetRandomNavmeshLocation();
		Loc.Z += 50.0f;
		if (BlueSapphireClass) {
			ACollectiblePickup* NewPickup = GetWorld()->SpawnActor<ACollectiblePickup>(BlueSapphireClass, Loc, FRotator::ZeroRotator);
			if (NewPickup) {
				NewPickup->SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
			}
		}
	}
}

void AFPSPickupSpawner::TryRespawnHealthPack() {
	ActiveHealthPacks.RemoveAll([](AActor* P) {
		return !IsValid(P);
		});

	if (ActiveHealthPacks.Num() < MaxActiveHealthPacks && HealthPackClass) {
		FVector Loc = GetRandomNavmeshLocation();
		Loc.Z += 50.0f;
		AActor* NewPickup = GetWorld()->SpawnActor<AHealthPackPickup>(HealthPackClass, Loc, FRotator::ZeroRotator);
		if (NewPickup) {
			NewPickup->SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
			ActiveHealthPacks.Add(NewPickup);
		}
	}
}

void AFPSPickupSpawner::TryRespawnAmmoCrate() {
	ActiveAmmoCrates.RemoveAll([](AActor* P) {
		return !IsValid(P);
		});

	if (ActiveAmmoCrates.Num() < MaxActiveAmmoCrates && AmmoCrateClass) {
		FVector Loc = GetRandomNavmeshLocation();
		Loc.Z += 50.0f;
		AActor* NewPickup = GetWorld()->SpawnActor<AAmmoCratePickup>(AmmoCrateClass, Loc, FRotator::ZeroRotator);
		if (NewPickup) {
			NewPickup->SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
			ActiveAmmoCrates.Add(NewPickup);
		}
	}
}

FVector AFPSPickupSpawner::GetRandomNavmeshLocation() const {
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return FVector::ZeroVector;
	FNavLocation NavLoc;
	FVector Origin = GetActorLocation();
	float Radius = 2000.f;
	if (NavSys->GetRandomReachablePointInRadius(Origin, Radius, NavLoc)) return NavLoc.Location;
	return Origin;
}

