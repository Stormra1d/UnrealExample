// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSWeaponBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "FPSCharacter.h"

// Sets default values
AFPSWeaponBase::AFPSWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.

	PrimaryActorTick.bCanEverTick = true;
	BulletsInMag = MagazineSize;

	MuzzleComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	MuzzleComponent->SetupAttachment(RootComponent);

}

// Called when the game starts or when spawned
void AFPSWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	BulletsInMag = MagazineSize;
	
}

// Called every frame
void AFPSWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool AFPSWeaponBase::CanFire() const {
	float TimeBetweenShots = 1.f / FireRate;
	float Now = GetWorld()->GetTimeSeconds();
	
	return !bIsReloading && BulletsInMag > 0 && (Now - LastFireTime) >= TimeBetweenShots;
}

void AFPSWeaponBase::Fire() {
	if (!CanFire()) {
		return;
	}

	BulletsInMag--;
	LastFireTime = GetWorld()->GetTimeSeconds();
	GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, FString::Printf(TEXT("%s fired! Bullets left: %d"), *WeaponName, BulletsInMag));

	AFPSCharacter* OwnerCharacter = Cast<AFPSCharacter>(GetOwner());
	if (OwnerCharacter) {
		OwnerCharacter->HandleWeaponFired(this);
	}

	if (BulletsInMag <= 0 && Magazines > 0) {
		Reload();
	}
}

void AFPSWeaponBase::Reload() {
	if (bIsReloading || Magazines <= 0 || BulletsInMag == MagazineSize)
		return;

	bIsReloading = true;
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, FString::Printf(TEXT("Reloading %s..."), *WeaponName));

	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AFPSWeaponBase::FinishReload, ReloadTime, false);
}

void AFPSWeaponBase::FinishReload() {
	int32 BulletsNeeded = MagazineSize - BulletsInMag;
	BulletsInMag += BulletsNeeded;
	Magazines -= 1;
	bIsReloading = false;
}

FVector AFPSWeaponBase::GetMuzzleWorldLocation() const {
	return MuzzleComponent ? MuzzleComponent->GetComponentLocation() : GetActorLocation();
}
