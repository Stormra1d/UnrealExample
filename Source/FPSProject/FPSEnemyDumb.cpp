// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSEnemyDumb.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "FPSCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AFPSEnemyDumb::AFPSEnemyDumb() {
	PrimaryActorTick.bCanEverTick = true;
}

void AFPSEnemyDumb::BeginPlay() {
	Super::BeginPlay();
}

void AFPSEnemyDumb::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	FVector PlayerLoc = GetPlayerLocation();
	FVector Direction = (PlayerLoc - GetActorLocation()).GetSafeNormal();
	float Speed = 400.0f;

	AddMovementInput(Direction, 1.0f);
}

FVector AFPSEnemyDumb::GetPlayerLocation() const {
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn && PlayerPawn->GetController()) {
		AFPSCharacter* FPSChar = Cast<AFPSCharacter>(PlayerPawn);
		if (FPSChar && !FPSChar->bIsDead && FPSChar->GetCurrentHealth() > 0) {
			return PlayerPawn->GetActorLocation();
		}
	}

	for (TActorIterator<AFPSCharacter> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator) {
		AFPSCharacter* FPSCharacter = *ActorIterator;
		if (FPSCharacter && IsValid(FPSCharacter) && !FPSCharacter->bIsDead && FPSCharacter->GetCurrentHealth() > 0) {
			if (FPSCharacter->GetController()) {
				return FPSCharacter->GetActorLocation();
			}
		}
	}

	return FVector::ZeroVector;
}