// Fill out your copyright notice in the Description page of Project Settings.

#include "FPSEnemySpawnManagerTests.h"
#include "CQTest.h"
#include "../FPSEnemySpawnManager.h"
#include "../FPSEnemyPatrol.h"
#include "../FPSEnemyDumb.h"
#include "Engine/World.h"
#include "TimerManager.h"

static void AdvanceTime(AFPSEnemySpawnManager* SUT, float Seconds, float Step = 0.0f)
{
    if (Step <= 0.f) { SUT->Tick(Seconds); return; }
    for (float t = 0.f; t < Seconds; t += Step) { SUT->Tick(Step); }
}

TEST_CLASS(SpawnManager_CQ_Showcase, "Game.Unit.EnemySpawning")
{
    UWorld* TestWorld;
    AFPSEnemySpawnManager* SUT;

    BEFORE_EACH()
    {
        FMath::RandInit(1337);

        TestWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TestWorld"));
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TestWorld);
        TestWorld->InitializeActorsForPlay(FURL());

        SUT = TestWorld->SpawnActor<AFPSEnemySpawnManager>(AFPSEnemySpawnManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        ASSERT_THAT(IsNotNull(SUT));

        SUT->SmartEnemy = AFPSEnemyPatrol::StaticClass();
        SUT->DumbEnemy = AFPSEnemyDumb::StaticClass();
        SUT->SpawnOrigin = FVector(100.0f, 100.0f, 0.0f);
        SUT->SpawnRadius = 1000.f;
        SUT->MaxDumbEnemiesPhase1 = 1;

        SUT->DispatchBeginPlay();
    }

    AFTER_EACH()
    {
        if (TestWorld)
        {
            TestWorld->DestroyWorld(true);
            TestWorld = nullptr;
        }
        SUT = nullptr;
    }

    TEST_METHOD(Phase1_SpawnsSmartEnemyOnBeginPlay)
    {
        ASSERT_THAT(IsNotNull(SUT->CurrentSmartEnemy));
        ASSERT_THAT(AreEqual(0, SUT->DumbEnemies.Num()));
    }

    TEST_METHOD(Phase1_SpawnsDumbEnemyEvery10Seconds)
    {
        AdvanceTime(SUT, 10.1f);
        ASSERT_THAT(AreEqual(1, SUT->DumbEnemies.Num()));
        ASSERT_THAT(IsNear(0.0f, SUT->DumbEnemySpawnTimer, 0.01f));
    }

    TEST_METHOD(Phase1_RespectsDumbEnemyCap)
    {
        AdvanceTime(SUT, 10.1f);
        AdvanceTime(SUT, 10.1f);
        ASSERT_THAT(AreEqual(1, SUT->DumbEnemies.Num()));
    }

    TEST_METHOD(Phase1_PartialTick_NoDumbEnemySpawn)
    {
        AdvanceTime(SUT, 5.0f);
        ASSERT_THAT(AreEqual(0, SUT->DumbEnemies.Num()));
        ASSERT_THAT(IsTrue(SUT->DumbEnemySpawnTimer > 0.0f));
    }

    TEST_METHOD(Phase2_TransitionsAfterThreeSmartEnemyDeaths)
    {
        for (int i = 0; i < 3; ++i)
        {
            auto* Smart = SUT->CurrentSmartEnemy;
            ASSERT_THAT(IsNotNull(Smart));
            SUT->NotifySmartEnemyDeath(Smart);
            if (i < 2)
            {
                ASSERT_THAT(AreEqual(ESpawnerPhase::Phase1, SUT->CurrentPhase));
                ASSERT_THAT(IsNotNull(SUT->CurrentSmartEnemy));
            }
        }

        ASSERT_THAT(AreEqual(ESpawnerPhase::Phase2, SUT->CurrentPhase));
        ASSERT_THAT(AreEqual(5, SUT->DumbEnemies.Num()));
        ASSERT_THAT(AreEqual(5, SUT->Phase2Spawned));
        ASSERT_THAT(IsTrue(SUT->GetWorldTimerManager().IsTimerActive(SUT->Phase2Handle)));
    }

    TEST_METHOD(Phase3_TransitionsAfterDumbEnemyDeaths)
    {
        for (int i = 0; i < 3; ++i)
        {
            SUT->NotifySmartEnemyDeath(SUT->CurrentSmartEnemy);
        }
        auto Copy = SUT->DumbEnemies;
        for (auto* E : Copy)
        {
            SUT->NotifyDumbEnemyDeath(E);
        }

        AdvanceTime(SUT, 0.016f);
        ASSERT_THAT(AreEqual(ESpawnerPhase::Phase3, SUT->CurrentPhase));
        ASSERT_THAT(AreEqual(0, SUT->DumbEnemies.Num()));
        ASSERT_THAT(AreEqual(0, SUT->Phase2Spawned));
    }

    TEST_METHOD(Phase3_AlternatesSmartAndDumbEnemySpawns)
    {
        for (int i = 0; i < 3; ++i)
        {
            SUT->NotifySmartEnemyDeath(SUT->CurrentSmartEnemy);
        }
        auto Copy = SUT->DumbEnemies;
        for (auto* E : Copy) { SUT->NotifyDumbEnemyDeath(E); }
        AdvanceTime(SUT, 0.016f);

        for (int i = 0; i < 6; ++i)
        {
            const int32 DumbBefore = SUT->DumbEnemies.Num();
            const bool HadSmart = (SUT->CurrentSmartEnemy != nullptr);

            AdvanceTime(SUT, 5.1f);

            const int32 DumbAfter = SUT->DumbEnemies.Num();
            const bool HasSmart = (SUT->CurrentSmartEnemy != nullptr);

            const bool DumbSpawned = (DumbAfter == DumbBefore + 1);
            const bool SmartSpawned = (!HadSmart && HasSmart);

            ASSERT_THAT(IsTrue(DumbSpawned || SmartSpawned));
            ASSERT_THAT(IsNear(0.0f, SUT->Phase3SpawnTimer, 0.01f));

            if (DumbAfter > 0 && SUT->CurrentSmartEnemy != nullptr)
            {
                break;
            }
        }
    }

    TEST_METHOD(Phase1_NonZeroSpawnRadius_EnemiesWithinBounds)
    {
        SUT->SpawnRadius = 1000.f;
        SUT->SpawnOrigin = FVector(100.f, 100.f, 0.f);

        AdvanceTime(SUT, 10.1f);
        ASSERT_THAT(AreEqual(1, SUT->DumbEnemies.Num()));
        ASSERT_THAT(IsNotNull(SUT->CurrentSmartEnemy));

        FVector SmartEnemyLocation = SUT->CurrentSmartEnemy->GetActorLocation();
        float Distance = FVector::Distance(SmartEnemyLocation, SUT->SpawnOrigin);
        ASSERT_THAT(IsTrue(Distance <= SUT->SpawnRadius));

        FVector DumbEnemyLocation = SUT->DumbEnemies[0]->GetActorLocation();
        Distance = FVector::Distance(DumbEnemyLocation, SUT->SpawnOrigin);
        ASSERT_THAT(IsTrue(Distance <= SUT->SpawnRadius));
    }

    TEST_METHOD(InvalidEnemyClasses_NoCrash)
    {
        if (SUT->CurrentSmartEnemy)
        {
            SUT->CurrentSmartEnemy->Destroy();
            SUT->CurrentSmartEnemy = nullptr;
        }

        for (auto* Enemy : SUT->DumbEnemies)
        {
            if (Enemy)
            {
                Enemy->Destroy();
            }
        }
        SUT->DumbEnemies.Empty();

        SUT->SmartEnemy = nullptr;
        SUT->DumbEnemy = nullptr;

        AdvanceTime(SUT, 10.1f);
        ASSERT_THAT(AreEqual(0, SUT->DumbEnemies.Num()));
        ASSERT_THAT(IsNull(SUT->CurrentSmartEnemy));
    }

    TEST_METHOD(Phase2_RapidEnemyDeaths_StableTransition)
    {
        for (int i = 0; i < 3; ++i)
        {
            auto* Smart = SUT->CurrentSmartEnemy;
            SUT->NotifySmartEnemyDeath(Smart);
        }

        ASSERT_THAT(AreEqual(ESpawnerPhase::Phase2, SUT->CurrentPhase));
        ASSERT_THAT(AreEqual(5, SUT->DumbEnemies.Num()));
    }

    TEST_METHOD(Phase1_NegativeSpawnRadius_HandlesGracefully)
    {
        SUT->SpawnRadius = -1000.f;

        AdvanceTime(SUT, 10.1f);
        ASSERT_THAT(AreEqual(1, SUT->DumbEnemies.Num()));
        ASSERT_THAT(IsNotNull(SUT->CurrentSmartEnemy));
    }

    TEST_METHOD(Phase1_ZeroMaxDumbEnemies_NoDumbSpawns)
    {
        SUT->MaxDumbEnemiesPhase1 = 0;

        AdvanceTime(SUT, 10.1f);
        ASSERT_THAT(AreEqual(0, SUT->DumbEnemies.Num()));
        ASSERT_THAT(IsNotNull(SUT->CurrentSmartEnemy));
    }
};