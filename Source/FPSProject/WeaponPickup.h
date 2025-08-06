// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "FPSWeaponBase.h"
#include "WeaponPickup.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AWeaponPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AWeaponPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	TSubclassOf<AFPSWeaponBase> WeaponClass;

	virtual void OnCollected(class AFPSCharacter* Character) override;
};
