// Fill out your copyright notice in the Description page of Project Settings.

#include "GameFramework/CharacterMovementComponent.h"
#include "FMinimalWorldLatentHelpers.h"
#include "FPSProject/FPSCharacter.h"
#include "FPSProject/HealthPackPickup.h"
#include "FPSProject/WeaponPickup.h"
#include <Tests/AutomationCommon.h>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCharacterHealthTest, "Game.FPSCharacter.Unit.Health", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSCharacterHealthTest::RunTest(const FString& Parameters) {
	ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(TEXT("/Game/Tests/MinimalTestMap")));

	ADD_LATENT_AUTOMATION_COMMAND(FSetUpWorldLatent([this](UWorld* World) {

		}));

	ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AFPSCharacter>(FVector::ZeroVector, FName(TEXT("/Game/Blueprint/BP_FPSCharacter.BP_FPSCharacter_C")), [this] (AFPSCharacter* Player) {
		TestNotNull(TEXT("Player should be spawned"), Player);
		if (!Player) return;

		Player->RegisterAllComponents();
		Player->DispatchBeginPlay();

		TestEqual(TEXT("Player starts at max health"), Player->GetCurrentHealth(), Player->MaxHealth);

		Player->ApplyDamage(50.0f);
		TestEqual(TEXT("Player health should be 50 after damage applied"), Player->GetCurrentHealth(), 50.0f);
		}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCharacterHealthPickupTest, "Game.FPSCharacter.Unit.HealthPickup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSCharacterHealthPickupTest::RunTest(const FString& Parameters) {
    TWeakObjectPtr<AFPSCharacter> Player = nullptr;
    TWeakObjectPtr<AHealthPackPickup> Pickup = nullptr;
    FVector SpawnedPickupLocation;

    ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(TEXT("/Game/Tests/MinimalTestMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FSetUpWorldLatent([](UWorld*) {}));

    ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AFPSCharacter>(
        FVector::ZeroVector,
        FName(TEXT("/Game/Blueprint/BP_FPSCharacter.BP_FPSCharacter_C")),
        [&Player, this](AFPSCharacter* SpawnedPlayer) {
            TestNotNull(TEXT("Player should be spawned"), SpawnedPlayer);
            if (!SpawnedPlayer) return;
            Player = SpawnedPlayer;
            SpawnedPlayer->RegisterAllComponents();
            SpawnedPlayer->DispatchBeginPlay();
            SpawnedPlayer->ApplyDamage(10.0f);
        }));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([&]() {
        if (Player.IsValid()) {
            FVector PickupLocation = Player->GetActorLocation() + FVector(100, 0, 0);
            ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AHealthPackPickup>(
                PickupLocation, FName(TEXT("/Game/Blueprint/BP_HealthPackPickup.BP_HealthPackPickup_C")),
                [&Pickup, &SpawnedPickupLocation, this](AHealthPackPickup* SpawnedPickup) {
                    TestNotNull(TEXT("HealthPackPickup should be spawned"), SpawnedPickup);
                    Pickup = SpawnedPickup;
                    if (SpawnedPickup)
                        SpawnedPickupLocation = SpawnedPickup->GetActorLocation();
                }
            ))
        }
        }, 0.1f));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([&]() {
        if (Player.IsValid() && Pickup.IsValid())
            Player->SetActorLocation(Pickup->GetActorLocation());
        }, 0.1f));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, &Player]() {
        if (Player.IsValid())
            TestEqual(TEXT("Player healed to max"), Player->GetCurrentHealth(), Player->MaxHealth);
        }, 0.1f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCharacterWeaponPickupTest, "Game.FPSCharacter.Unit.WeaponPickup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSCharacterWeaponPickupTest::RunTest(const FString& Parameters) {
    TWeakObjectPtr<AFPSCharacter> Player = nullptr;
    TWeakObjectPtr<AWeaponPickup> Pickup = nullptr;
    FVector SpawnedPickupLocation;

    ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(TEXT("/Game/Tests/MinimalTestMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FSetUpWorldLatent([](UWorld*) {}));

    ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AFPSCharacter>(
        FVector::ZeroVector,
        FName(TEXT("/Game/Blueprint/BP_FPSCharacter.BP_FPSCharacter_C")),
        [&Player, this](AFPSCharacter* SpawnedPlayer) {
            TestNotNull(TEXT("Player should be spawned"), SpawnedPlayer);
            if (!SpawnedPlayer) return;
            Player = SpawnedPlayer;
            SpawnedPlayer->RegisterAllComponents();
            SpawnedPlayer->DispatchBeginPlay();
        }));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([&]() {
        if (Player.IsValid()) {
            FVector PickupLocation = Player->GetActorLocation() + FVector(100, 0, 0);
            ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AWeaponPickup>(
                PickupLocation, FName(TEXT("/Game/Blueprint/BP_RiflePickup.BP_RiflePickup_C")),
                [&Pickup, &SpawnedPickupLocation, this](AWeaponPickup* SpawnedPickup) {
                    TestNotNull(TEXT("WeaponPickup should be spawned"), SpawnedPickup);
                    Pickup = SpawnedPickup;
                    if (SpawnedPickup)
                        SpawnedPickupLocation = SpawnedPickup->GetActorLocation();
                }
            ))
        }
        }, 0.1f));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([&]() {
        if (Player.IsValid() && Pickup.IsValid())
            Player->SetActorLocation(Pickup->GetActorLocation());
        }, 0.1f));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, &Player]() {
        if (Player.IsValid()) {
            TestNotNull(TEXT("Primary weapon should now be set"), Player->GetPrimaryWeapon());
            TestEqual(TEXT("Equipped weapon is primary"), Player->GetEquippedWeapon(), Player->GetPrimaryWeapon());
        }
        }, 0.1f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCharacterCollectibleAchievementTest, "Game.FPSCharacter.Unit.CollectibleAchievement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSCharacterCollectibleAchievementTest::RunTest(const FString& Parameters) {
    TWeakObjectPtr<AFPSCharacter> Player = nullptr;

    ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(TEXT("/Game/Tests/MinimalTestMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FSetUpWorldLatent([](UWorld*) {}));

    ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AFPSCharacter>(
        FVector::ZeroVector,
        FName(TEXT("/Game/Blueprint/BP_FPSCharacter.BP_FPSCharacter_C")),
        [&Player, this](AFPSCharacter* SpawnedPlayer) {
            TestNotNull(TEXT("Player should be spawned"), SpawnedPlayer);
            if (!SpawnedPlayer) return;
            Player = SpawnedPlayer;
            SpawnedPlayer->RegisterAllComponents();
            SpawnedPlayer->DispatchBeginPlay();
        }));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, &Player]() {
        if (Player.IsValid()) {
            for (int i = 0; i < 5; i++) {
                Player->CollectItem(ECollectibleType::RedRuby);
            }
            for (int i = 0; i < 5; i++) {
                Player->CollectItem(ECollectibleType::BlueSapphire);
            }

            TestTrue(TEXT("All collectible achievement unlocked"), Player->bUnlockedAllCollectibles);
        }
        }, 0.1f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCharacterSprintTest, "Game.FPSCharacter.Unit.SprintSpeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSCharacterSprintTest::RunTest(const FString& Parameters) {
    TWeakObjectPtr<AFPSCharacter> Player = nullptr;

    ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(TEXT("/Game/Tests/MinimalTestMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FSetUpWorldLatent([](UWorld*) {}));

    ADD_LATENT_AUTOMATION_COMMAND(FSpawnActorLatent<AFPSCharacter>(
        FVector::ZeroVector,
        FName(TEXT("/Game/Blueprint/BP_FPSCharacter.BP_FPSCharacter_C")),
        [&Player, this](AFPSCharacter* SpawnedPlayer) {
            TestNotNull(TEXT("Player should be spawned"), SpawnedPlayer);
            if (!SpawnedPlayer) return;
            Player = SpawnedPlayer;
            SpawnedPlayer->RegisterAllComponents();
            SpawnedPlayer->DispatchBeginPlay();
        }));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, &Player]() {
        if (Player.IsValid()) {
            FInputActionValue SprintPressed(true);
            Player->Sprint(SprintPressed);
            TestTrue(TEXT("Sprinting sets speed"), Player->GetCharacterMovement()->MaxWalkSpeed == Player->SprintSpeed);

            FInputActionValue SprintReleased(false);
            Player->Sprint(SprintReleased);
            TestTrue(TEXT("Not sprinting sets speed"), Player->GetCharacterMovement()->MaxWalkSpeed == Player->WalkSpeed);
        }
        }, 0.1f));

    return true;
}