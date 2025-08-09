#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "../FPSCharacter.h"
#include "../FPSEnemyBase.h"
#include "../FPSPickupSpawner.h"
#include <Tests/AutomationCommon.h>


struct FOpenMapLatentCommand : public IAutomationLatentCommand
{
    FString MapName;
    FOpenMapLatentCommand(const FString& InMapName)
        : MapName(InMapName) {
    }

    virtual bool Update() override
    {
        return AutomationOpenMap(MapName);
    }
};

struct FSetUpWorldLatent : public IAutomationLatentCommand
{
    TFunction<void(UWorld*)> OnReady;
    FSetUpWorldLatent(TFunction<void(UWorld*)> InOnReady)
        : OnReady(InOnReady) {
    }

    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE)
            {
                World = Context.World();
                break;
            }
        }
        if (!World) return false;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0, 0, 0), FRotator::ZeroRotator, Params);
        if (Floor)
        {
            UStaticMesh* PlaneMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
            if (PlaneMesh)
            {
                Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                Floor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
                Floor->SetActorScale3D(FVector(40, 40, 1));
            }
        }

        ANavMeshBoundsVolume* NavBounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 100), FRotator::ZeroRotator, Params);
        if (NavBounds)
        {
            NavBounds->GetRootComponent()->SetMobility(EComponentMobility::Static);
            NavBounds->SetActorScale3D(FVector(4000, 4000, 2000) / 100.f);
        }

        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (NavSys)
        {
            NavSys->Build();
            while (NavSys->IsNavigationBuildInProgress())
            {
                World->Tick(LEVELTICK_All, 0.1f);
            }
        }

        for (int i = 0; i < 3; ++i)
            World->Tick(LEVELTICK_All, 1.0f / 60.0f);

        if (OnReady) OnReady(World);

        return true;
    }
};

template<typename T>
struct FSpawnActorLatent : public IAutomationLatentCommand
{
    FVector Location;
    FName BlueprintPath;
    TFunction<void(T*)> OnSpawned;
    FSpawnActorLatent(const FVector& InLocation, const FName& InBlueprintPath, TFunction<void(T*)> InOnSpawned)
        : Location(InLocation), BlueprintPath(InBlueprintPath), OnSpawned(InOnSpawned) {
    }

    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE)
            {
                World = Context.World();
            }
        }
        if (!World) return false;

        UClass* SpawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *BlueprintPath.ToString()));
        if (!SpawnClass) return false;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        T* Actor = World->SpawnActor<T>(SpawnClass, Location, FRotator::ZeroRotator, Params);
        if (OnSpawned) OnSpawned(Actor);

        return true;
    }
};
