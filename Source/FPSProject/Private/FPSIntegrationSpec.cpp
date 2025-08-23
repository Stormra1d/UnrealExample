#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/CapsuleComponent.h"
#include "../FPSCharacter.h"
#include "../FPSWeaponBase.h"
#include "../AmmoCratePickup.h"
#include "../FPSHUD.h"
#include "../FPSEnemyBase.h"
#include "../FPSEnemyDumb.h"
#include "FunctionalTest.h"

DEFINE_SPEC(FPSIntegrationSpec, "Game.FPS.Integration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

void FPSIntegrationSpec::Define() {
    Describe("AmmoCratePickup Integration", [this]() {
        AFPSCharacter* Player = nullptr;
        AFPSWeaponBase* PrimaryWeapon = nullptr;
        AFPSWeaponBase* SecondaryWeapon = nullptr;
        AAmmoCratePickup* AmmoPickup = nullptr;
        TStrongObjectPtr<UWorld> World(nullptr);

        BeforeEach([this, &Player, &PrimaryWeapon, &SecondaryWeapon, &AmmoPickup, &World]() {
            World.Reset(FAutomationEditorCommonUtils::CreateNewMap());
            TestNotNull("World exists", World.Get());
            if (!World.Get()) return;

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            Player = World->SpawnActor<AFPSCharacter>(AFPSCharacter::StaticClass(), FTransform::Identity, SpawnParams);
            TestNotNull("Player exists", Player);
            if (!Player) return;

            PrimaryWeapon = World->SpawnActor<AFPSWeaponBase>(AFPSWeaponBase::StaticClass(), FTransform::Identity, SpawnParams);
            SecondaryWeapon = World->SpawnActor<AFPSWeaponBase>(AFPSWeaponBase::StaticClass(), FTransform::Identity, SpawnParams);

            TestNotNull("Primary weapon exists", PrimaryWeapon);
            TestNotNull("Secondary weapon exists", SecondaryWeapon);

            if (!PrimaryWeapon || !SecondaryWeapon) return;

            if (Player && PrimaryWeapon && SecondaryWeapon) {
                Player->SetPrimaryWeapon(PrimaryWeapon);
                Player->SetSecondaryWeapon(SecondaryWeapon);
                Player->SetEquippedWeapon(PrimaryWeapon);

                PrimaryWeapon->Magazines = 1;
                SecondaryWeapon->Magazines = 2;
            }

            AmmoPickup = World->SpawnActor<AAmmoCratePickup>(AAmmoCratePickup::StaticClass(), FTransform(FVector(300, 0, 500)), SpawnParams);
            TestNotNull("Ammo pickup exists", AmmoPickup);
            });

        AfterEach([this, &World, &Player, &PrimaryWeapon, &SecondaryWeapon, &AmmoPickup]() {
            if (Player) Player->Destroy();
            if (PrimaryWeapon) PrimaryWeapon->Destroy();
            if (SecondaryWeapon) SecondaryWeapon->Destroy();
            if (AmmoPickup) AmmoPickup->Destroy();
            if (World.Get()) World->DestroyWorld(true);
            World.Reset();
            });

        It("should increase magazines on both weapons when collected", [this, &Player, &PrimaryWeapon, &SecondaryWeapon, &AmmoPickup]() {
            TestTrue("Setup valid", Player && AmmoPickup && PrimaryWeapon && SecondaryWeapon && IsValid(Player) && IsValid(AmmoPickup) && IsValid(PrimaryWeapon) && IsValid(SecondaryWeapon));
            if (!Player || !AmmoPickup || !PrimaryWeapon || !SecondaryWeapon) return;

            AmmoPickup->OnCollected(Player);

            TestEqual("Primary weapon magazines increased", PrimaryWeapon->Magazines, 2);
            TestEqual("Secondary weapon magazines increased", SecondaryWeapon->Magazines, 3);
            });
        });

    Describe("Enemy Contact Damage Integration", [this]() {
        TStrongObjectPtr<UWorld> World(nullptr);
        AFPSCharacter* Player = nullptr;
        AFPSEnemyBase* Enemy = nullptr;

        BeforeEach([this, &World, &Player, &Enemy]() {
            World.Reset(FAutomationEditorCommonUtils::CreateNewMap());
            TestNotNull("World exists", World.Get());
            if (!World.Get()) return;

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            Player = World->SpawnActor<AFPSCharacter>(AFPSCharacter::StaticClass(), FTransform::Identity, SpawnParams);
            TestNotNull("Player exists", Player);

            Enemy = World->SpawnActor<AFPSEnemyBase>(AFPSEnemyBase::StaticClass(), FTransform::Identity, SpawnParams);
            TestNotNull("Enemy exists", Enemy);

            if (Player && Enemy) {
                Player->MaxHealth = 100.0f;
                Player->CurrentHealth = 100.0f;
                Enemy->ContactDamage = 25.0f;

                if (UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent()) {
                    PlayerCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                }
                if (UCapsuleComponent* EnemyCapsule = Enemy->GetCapsuleComponent()) {
                    EnemyCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                }
            }
            });

        AfterEach([this, &World, &Player, &Enemy]() {
            if (Player) Player->Destroy();
            if (Enemy) Enemy->Destroy();

            if (World.Get()) {
                World->DestroyWorld(true);
            }
            World.Reset();

            Player = nullptr;
            Enemy = nullptr;
            });

        It("should apply damage when overlap occurs", [this, &Player, &Enemy]() {
            TestTrue("Setup valid", Player && Enemy && IsValid(Player) && IsValid(Enemy));
            if (!Player || !Enemy) return;

            Enemy->OnOverlapBegin(nullptr, Player, nullptr, 0, false, FHitResult());

            TestNearlyEqual("Player health reduced by contact damage", Player->CurrentHealth, 75.0f, 0.001f);
            });

        LatentIt("should apply damage over time", EAsyncExecution::ThreadPool, [this, &Player, &Enemy](const FDoneDelegate& Done) {
            TestTrue("Setup valid", Player && Enemy && IsValid(Player) && IsValid(Enemy));
            if (!Player || !Enemy) {
                Done.Execute();
                return;
            }

            Enemy->OnOverlapBegin(nullptr, Player, nullptr, 0, false, FHitResult());

            ADD_LATENT_AUTOMATION_COMMAND(
                FDelayedFunctionLatentCommand(
                    [this, &Player, Done]()
                    {
                        TestNearlyEqual("Player health reduced after tick", Player->CurrentHealth, 75.0f, 0.001f);
                        Done.Execute();
                    },
                    0.0f
                )
            );
        });
     });
}
#endif