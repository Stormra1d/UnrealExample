// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FPSEnemyBase.generated.h"

UCLASS()
class FPSPROJECT_API AFPSEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AFPSEnemyBase();
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy")
	float MaxHealth = 40.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy")
	float CurrentHealth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy")
	float ContactDamage = 20.f;

	//TODO 
	//UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy")
	//float KnockbackStrength = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	bool bFlashOnHit = true;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterialInstance;

	FTimerHandle FlashTimerHandle;

	//POT TODO KNOCKBACK
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	virtual void ReceiveDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	virtual void OnDeath();

	void ApplyKnockback(const FVector& Direction, float Strength);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	class AFPSEnemySpawnManager* OwningSpawner = nullptr;

protected:
	void StartHitFlash();
	void EndHitFlash();

};
