// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSPickupSpawner.generated.h"

UCLASS()
class FPSPROJECT_API AFPSPickupSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFPSPickupSpawner();

	UPROPERTY(EditAnywhere, Category = Spawning)
	int32 NumberRubies = 5;

	UPROPERTY(EditAnywhere, Category = Spawning)
	int32 NumberSapphires = 5;

	UPROPERTY(EditAnywhere, Category = Spawning)
	int32 MaxActiveHealthPacks = 5;

	UPROPERTY(EditAnywhere, Category = Spawning)
	int32 MaxActiveAmmoCrates = 5;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class ACollectiblePickup> RedRubyClass;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class ACollectiblePickup> BlueSapphireClass;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class AHealthPackPickup> HealthPackClass;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class AAmmoCratePickup> AmmoCrateClass;

	UPROPERTY(EditAnywhere, Category = Spawning)
	float RespawnTime = 60.0f;

	TArray<AActor*> ActiveHealthPacks;
	TArray<AActor*> ActiveAmmoCrates;

	float HealthPackSpawnTimer = 0.0f;
	float AmmoCrateSpawnTimer = 0.0f;

	void SpawnCollectibles();
	void TryRespawnHealthPack();
	void TryRespawnAmmoCrate();
	FVector GetRandomNavmeshLocation() const;
};
