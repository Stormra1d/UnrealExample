// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "AmmoCratePickup.generated.h"

UCLASS()
class FPSPROJECT_API AAmmoCratePickup : public APickupBase
{
	GENERATED_BODY()

public:
	virtual void OnCollected(class AFPSCharacter* Character) override;
	
};
