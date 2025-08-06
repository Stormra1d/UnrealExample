// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthPackPickup.h"
#include "FPSCharacter.h"

void AHealthPackPickup::OnCollected(AFPSCharacter* Character) {
	if (!Character) return;
	Character->Heal(25.0f);
}