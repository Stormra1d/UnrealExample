//TODO rewrite + not good cleanup (deletes my current world, doesn't save ofc)

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/CapsuleComponent.h"
#include "../FPSCharacter.h"
#include "../FPSWeaponBase.h"
#include "../AmmoCratePickup.h"
#include "../FPSHUD.h"
#include "../FPSEnemyBase.h"
#include "../FPSEnemyDumb.h"

DEFINE_SPEC(FPSIntegrationSpec, "Game.FPS.Integration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

void FPSIntegrationSpec::Define() {
    // AmmoCratePickup Integration Tests
    Describe("AmmoCratePickup Integration", [this]() {
        AFPSCharacter* Player = nullptr;
        AFPSWeaponBase* PrimaryWeapon = nullptr;
        AFPSWeaponBase* SecondaryWeapon = nullptr;
        AAmmoCratePickup* AmmoPickup = nullptr;
        UWorld* World = nullptr;

        BeforeEach([this, &Player, &PrimaryWeapon, &SecondaryWeapon, &AmmoPickup, &World]() {
            // Create a new world for the test
            World = FAutomationEditorCommonUtils::CreateNewMap();
            TestNotNull("World exists", World);
            if (!World) return;

            // Spawn parameters
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // Spawn player
            Player = World->SpawnActor<AFPSCharacter>(AFPSCharacter::StaticClass(), FTransform::Identity, SpawnParams);
            TestNotNull("Player exists", Player);
            if (!Player) return;

            // Spawn weapons
            PrimaryWeapon = World->SpawnActor<AFPSWeaponBase>(AFPSWeaponBase::StaticClass(), FTransform::Identity, SpawnParams);
            SecondaryWeapon = World->SpawnActor<AFPSWeaponBase>(AFPSWeaponBase::StaticClass(), FTransform::Identity, SpawnParams);

            if (!PrimaryWeapon || !SecondaryWeapon) return;

            Player->SetPrimaryWeapon(PrimaryWeapon);
            Player->SetSecondaryWeapon(SecondaryWeapon);
            Player->SetEquippedWeapon(PrimaryWeapon);

            PrimaryWeapon->Magazines = 1;
            SecondaryWeapon->Magazines = 2;

            // Spawn ammo pickup
            AmmoPickup = World->SpawnActor<AAmmoCratePickup>(AAmmoCratePickup::StaticClass(), FTransform(FVector(300, 0, 500)), SpawnParams);
            TestNotNull("Ammo pickup exists", AmmoPickup);
            if (!AmmoPickup) return;
            });

        AfterEach([&]() {
            Player = nullptr;
            PrimaryWeapon = nullptr;
            SecondaryWeapon = nullptr;
            AmmoPickup = nullptr;
            World = nullptr;
            });

        It("should increase magazines on both weapons when collected", [this, &Player, &PrimaryWeapon, &SecondaryWeapon, &AmmoPickup]() {
            if (!Player || !AmmoPickup || !PrimaryWeapon || !SecondaryWeapon) return;
            if (!IsValid(Player) || !IsValid(AmmoPickup) || !IsValid(PrimaryWeapon) || !IsValid(SecondaryWeapon)) return;

            AmmoPickup->OnCollected(Player);

            TestEqual("Primary weapon magazines increased", PrimaryWeapon->Magazines, 2);
            TestEqual("Secondary weapon magazines increased", SecondaryWeapon->Magazines, 3);
            });
        });

    // Enemy Contact Damage Integration Tests
    Describe("Enemy Contact Damage Integration", [this]() {
        It("should apply damage when overlap occurs", [this]() {
            UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
            if (!World) return;

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AFPSCharacter* Player = World->SpawnActor<AFPSCharacter>(AFPSCharacter::StaticClass(), FTransform::Identity, SpawnParams);
            AFPSEnemyBase* Enemy = World->SpawnActor<AFPSEnemyBase>(AFPSEnemyBase::StaticClass(), FTransform::Identity, SpawnParams);

            if (!Player || !Enemy) {
                if (Player) Player->Destroy();
                if (Enemy) Enemy->Destroy();
                return;
            }

            // Initialize test values
            Player->MaxHealth = 100.0f;
            Player->CurrentHealth = 100.0f;
            Enemy->ContactDamage = 25.0f;

            // Test the overlap logic directly
            Enemy->OnOverlapBegin(nullptr, Player, nullptr, 0, false, FHitResult());

            // Verify damage was applied
            TestEqual("Player health reduced by contact damage", Player->CurrentHealth, 75.0f);

            // Cleanup
            Player->Destroy();
            Enemy->Destroy();
            });
        });
}