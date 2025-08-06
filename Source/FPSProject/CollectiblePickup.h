// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "CollectiblePickup.generated.h"

UENUM(BlueprintType)
enum class ECollectibleType : uint8 {
	RedRuby UMETA(DisplayName = "Red Ruby"),
	BlueSapphire UMETA(DisplayName = "Blue Sapphire"),
	FinishToken,
};


UCLASS()
class FPSPROJECT_API ACollectiblePickup : public APickupBase
{
	GENERATED_BODY()

public:
	ACollectiblePickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible")
	ECollectibleType CollectibleType = ECollectibleType::RedRuby;

	virtual void OnCollected(class AFPSCharacter* Character) override;
	
};
