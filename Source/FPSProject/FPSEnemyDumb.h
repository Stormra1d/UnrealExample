// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FPSEnemyBase.h"
#include "FPSEnemyDumb.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AFPSEnemyDumb : public AFPSEnemyBase
{
	GENERATED_BODY()

public: 
	AFPSEnemyDumb();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	FVector GetPlayerLocation() const;
	
};
