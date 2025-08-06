// Fill out your copyright notice in the Description page of Project Settings.


#include "BotTargetPlanner.h"
#include "../FPSCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "../FPSEnemyBase.h"
#include "../AmmoCratePickup.h"
#include "../HealthPackPickup.h"
#include "../WeaponPickup.h"
#include "../CollectiblePickup.h"
#include <NavigationSystem.h>
#include <PlayerAIController.h>

bool UBotTargetPlanner::EvaluateBestTarget(APlayerAIController* Controller, AFPSCharacter* Player, AActor*& OutTarget) {
    if (!Player) {
        UE_LOG(LogTemp, Error, TEXT("[TargetPlanner] No valid player"));
        return false;
    }

    const FVector PlayerLocation = Player->GetActorLocation();

    if (Controller && Controller->bShouldEngageEnemy) {
        UE_LOG(LogTemp, Warning, TEXT("[TargetPlanner] Skipping targeting due to enemy engagement"));
        return false;
    }

    if (!Player->GetEquippedWeapon()) {
        OutTarget = FindNearestPickupOfType(AWeaponPickup::StaticClass(), PlayerLocation);
        if (OutTarget) {
            UE_LOG(LogTemp, Log, TEXT("[TargetPlanner] Targeting WeaponPickup: %s"), *OutTarget->GetName());
            return true;
        }
        UE_LOG(LogTemp, Warning, TEXT("[TargetPlanner] No weapon found and no weapon pickup nearby"));
    }

    if (Player->GetCurrentHealth() < 25.f) {
        OutTarget = FindNearestPickupOfType(AHealthPackPickup::StaticClass(), PlayerLocation);
        if (OutTarget) {
            UE_LOG(LogTemp, Log, TEXT("[TargetPlanner] Targeting HealthPack: %s"), *OutTarget->GetName());
            return true;
        }
        UE_LOG(LogTemp, Warning, TEXT("[TargetPlanner] Low health but no health pack found"));
    }

    if (Player->GetEquippedWeapon() && Player->GetEquippedWeapon()->Magazines == 0) {
        OutTarget = FindNearestPickupOfType(AAmmoCratePickup::StaticClass(), PlayerLocation);
        if (OutTarget) {
            UE_LOG(LogTemp, Log, TEXT("[TargetPlanner] Targeting AmmoCrate: %s"), *OutTarget->GetName());
            return true;
        }
        UE_LOG(LogTemp, Warning, TEXT("[TargetPlanner] No magazines left but no ammo found"));
    }

    OutTarget = FindNearestEnemy(PlayerLocation);
    if (OutTarget) {
        UE_LOG(LogTemp, Log, TEXT("[TargetPlanner] Targeting Enemy: %s"), *OutTarget->GetName());
        return true;
    }

    OutTarget = FindRandomCollectible(PlayerLocation);
    if (OutTarget) {
        UE_LOG(LogTemp, Log, TEXT("[TargetPlanner] Targeting Random Collectible: %s"), *OutTarget->GetName());
        return true;
    }

    // Fallback
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Player->GetWorld());
    if (NavSys) {
        FNavLocation RandomLoc;
        if (NavSys->GetRandomReachablePointInRadius(PlayerLocation, 1000.0f, RandomLoc)) {
            AActor* Dummy = NewObject<AActor>(Player->GetWorld());
            Dummy->SetActorLocation(RandomLoc.Location);
            OutTarget = Dummy;
            UE_LOG(LogTemp, Log, TEXT("[TargetPlanner] Using fallback nav location"));
            return true;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("[TargetPlanner] No target found at all"));
    return false;
}



AActor* UBotTargetPlanner::FindNearestPickupOfType(UClass* PickupClass, const FVector& From) {
	TArray<AActor*> Pickups;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), PickupClass, Pickups);

	float BestDist = TNumericLimits<float>::Max();
	AActor* Best = nullptr;

	for (AActor* Actor : Pickups) {
		float Dist = FVector::DistSquared(Actor->GetActorLocation(), From);
		if (Dist < BestDist) {
			BestDist = Dist;
			Best = Actor;
		}
	}
	return Best;
}

AActor* UBotTargetPlanner::FindNearestEnemy(const FVector& From) {
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFPSEnemyBase::StaticClass(), Enemies);

	AActor* Best = nullptr;
	float Closest = 2000.f * 2000.f;

	for (AActor* Enemy : Enemies) {
		float Dist = FVector::DistSquared(From, Enemy->GetActorLocation());
		if (Dist < Closest) {
			Closest = Dist;
			Best = Enemy;
		}
	}
	return Best;
}

AActor* UBotTargetPlanner::FindRandomCollectible(const FVector& From) {
	TArray<AActor*> Collectibles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACollectiblePickup::StaticClass(), Collectibles);

	return Collectibles.Num() > 0 ? Collectibles[FMath::RandHelper(Collectibles.Num())] : nullptr;
}