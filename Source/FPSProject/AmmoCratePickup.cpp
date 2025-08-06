// Fill out your copyright notice in the Description page of Project Settings.


#include "AmmoCratePickup.h"
#include "FPSCharacter.h"
#include "FPSWeaponBase.h"

void AAmmoCratePickup::OnCollected(AFPSCharacter* Character) {
	if (!Character) return;
	if (Character->GetPrimaryWeapon()) {
		Character->GetPrimaryWeapon()->Magazines += 1;
	}
	if (Character->GetSecondaryWeapon()) {
		Character->GetSecondaryWeapon()->Magazines += 1;
	}
}
