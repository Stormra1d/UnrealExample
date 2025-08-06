// Fill out your copyright notice in the Description page of Project Settings.


#include "CollectiblePickup.h"
#include "FPSCharacter.h"

ACollectiblePickup::ACollectiblePickup() {

}

void ACollectiblePickup::OnCollected(AFPSCharacter* Character) {
	if (!Character) return;
	Character->CollectItem(CollectibleType);
}