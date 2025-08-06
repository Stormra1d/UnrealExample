// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSCharacter.h"
#include "PoisonTile.generated.h"

UCLASS()
class FPSPROJECT_API APoisonTile : public AActor
{
	GENERATED_BODY()
	
public:	
	APoisonTile();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UBoxComponent* BoxComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DamagePerSecond = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDuration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FColor BoxColor = FColor::Green;

	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* MeshComponent;

	TMap<AFPSCharacter*, float> PoisonTimer;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

};
