// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponPickup.h"
#include "FPSCharacter.h"
#include "FPSWeaponBase.h"
#include "Engine/World.h"

AWeaponPickup::AWeaponPickup() {
	//TODO if anything set a default mesh for vis representation
}

void AWeaponPickup::OnCollected(AFPSCharacter* Character) {
	if (!Character || !WeaponClass) return;

	if (!Character->GetPrimaryWeapon()) {
		UE_LOG(LogTemp, Warning, TEXT("OnCollected PrimaryWeapon fired for %s"), *GetName());
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		AFPSWeaponBase* NewWeapon = GetWorld()->SpawnActor<AFPSWeaponBase>(WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (NewWeapon) {
			UE_LOG(LogTemp, Warning, TEXT("NEW WEAPON fired for %s"), *GetName());
			Character->SetPrimaryWeapon(NewWeapon);
			Character->SetEquippedWeapon(NewWeapon);
			NewWeapon->AttachToComponent(Character->FPSCameraComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			NewWeapon->SetActorRelativeLocation(FVector(50.0f, 20.0f, -20.0f));
			NewWeapon->SetActorRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
	else if (!Character->GetSecondaryWeapon()) {
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		AFPSWeaponBase* NewWeapon = GetWorld()->SpawnActor<AFPSWeaponBase>(WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (NewWeapon) {
			Character->SetSecondaryWeapon(NewWeapon);
		}
	}
	//TODO: Could consider dropping secondary weapon if both slots are full
}


