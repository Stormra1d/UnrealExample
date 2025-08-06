// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSWeaponBase.generated.h"

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	SemiAuto UMETA(DisplayName="Semi-Auto"),
	FullAuto UMETA(DisplayName="Full-Auto")
};

UCLASS(Blueprintable)
class FPSPROJECT_API AFPSWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFPSWeaponBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	FString WeaponName = "DefaultWeapon";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float Damage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	EWeaponFireMode FireMode = EWeaponFireMode::SemiAuto;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float FireRate = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	int32 MagazineSize = 7;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Weapon")
	int32 BulletsInMag = 7;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	int32 Magazines = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float ReloadTime = 1.0f;

	bool bIsReloading = false;
	float LastFireTime = -1000.0f;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool CanFire() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Reload();

	int32 GetTotalReserveBullets() const {
		return Magazines * MagazineSize;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* MuzzleComponent;

	FVector GetMuzzleWorldLocation() const;

protected:
	FTimerHandle ReloadTimerHandle;
	void FinishReload();
};
