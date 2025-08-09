// Fill out your copyright notice in the Description page of Project Settings.

#include "FPSProjectGameModeBase.h"
#include "./Private/PlayerAIController.h"
#include "FPSCharacter.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include <Kismet/GameplayStatics.h>
#include <BotTargetPlanner.h>
#include <BotTestMonitorSubsystem.h>
#include "Engine/Engine.h"

void AFPSProjectGameModeBase::StartPlay()
{
    Super::StartPlay();

    BotTargetPlanner = NewObject<UBotTargetPlanner>(this);

    bEnablePlayerAI = FParse::Param(FCommandLine::Get(), TEXT("AITest"));
    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("GAME MODE StartPlay"));

    if (bEnablePlayerAI)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("ENABLED PLAYER AI"));

        // Check if we're in automation test mode
        bool bIsAutomationTest = GIsAutomationTesting;

        if (bIsAutomationTest) {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, TEXT("AUTOMATION TEST MODE DETECTED"));
            // In automation test mode, delay setup slightly more
            FTimerHandle TimerHandle;
            GetWorldTimerManager().SetTimer(TimerHandle, this, &AFPSProjectGameModeBase::SetupPlayerAI, 1.0f, false);
        }
        else {
            // Normal standalone mode
            FTimerHandle TimerHandle;
            GetWorldTimerManager().SetTimer(TimerHandle, this, &AFPSProjectGameModeBase::SetupPlayerAI, 0.1f, false);
        }
    }
}

void AFPSProjectGameModeBase::SetupPlayerAI()
{
    UE_LOG(LogTemp, Log, TEXT("SetupPlayerAI called"));

    // Clear previous cached references
    CachedPlayerCharacter = nullptr;
    CachedAIController = nullptr;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
    AFPSCharacter* PlayerCharacter = Cast<AFPSCharacter>(PlayerPawn);

    if (PC && PlayerPawn && PlayerCharacter)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, TEXT("Setting up AI - Found valid player"));
        UE_LOG(LogTemp, Log, TEXT("Found valid player: %s"), *PlayerCharacter->GetName());

        CachedPlayerCharacter = PlayerCharacter;
        PlayerCharacter->bIsTestMode = true;

        // Check if player is already possessed by AI (in case of level transition)
        APlayerAIController* ExistingAI = Cast<APlayerAIController>(PlayerCharacter->GetController());
        if (ExistingAI)
        {
            UE_LOG(LogTemp, Log, TEXT("Player already has AI controller, updating references"));
            CachedAIController = ExistingAI;

            // Update monitor with current player
            UGameInstance* GameInstance = GetWorld()->GetGameInstance();
            UBotTestMonitorSubsystem* Monitor = GameInstance ? GameInstance->GetSubsystem<UBotTestMonitorSubsystem>() : nullptr;
            if (Monitor)
            {
                Monitor->SetTestPlayerPawn(PlayerCharacter);
                UE_LOG(LogTemp, Log, TEXT("Monitor updated with existing AI player"));
            }

            // Update timer
            GetWorldTimerManager().ClearTimer(TargetUpdateTimerHandle);
            GetWorldTimerManager().SetTimer(TargetUpdateTimerHandle, this, &AFPSProjectGameModeBase::UpdateBotTarget, 2.0f, true);
            return;
        }

        // Only unpossess if not already AI controlled
        if (!Cast<APlayerAIController>(PC))
        {
            PC->UnPossess();
            PC->ChangeState(NAME_Spectating);
            PC->bAutoManageActiveCameraTarget = false;
            PC->SetViewTargetWithBlend(PlayerPawn, 0.0f);
        }

        APlayerAIController* AIController = GetWorld()->SpawnActor<APlayerAIController>();
        if (AIController)
        {
            AIController->Possess(PlayerPawn);
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("AI POSSESSED PLAYER"));
            UE_LOG(LogTemp, Log, TEXT("AIController possessed pawn: %s"), *PlayerPawn->GetName());

            UGameInstance* GameInstance = GetWorld()->GetGameInstance();
            UBotTestMonitorSubsystem* Monitor = GameInstance ? GameInstance->GetSubsystem<UBotTestMonitorSubsystem>() : nullptr;

            if (Monitor && PlayerCharacter) {
                Monitor->SetTestPlayerPawn(PlayerCharacter);
                GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Monitor configured with player"));
                UE_LOG(LogTemp, Log, TEXT("Monitor configured with player: %s"), *PlayerCharacter->GetName());
            }
            else {
                GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("ERROR: Monitor or PlayerCharacter null"));
                UE_LOG(LogTemp, Error, TEXT("Monitor=%s, PlayerCharacter=%s"),
                    Monitor ? TEXT("Valid") : TEXT("NULL"),
                    PlayerCharacter ? *PlayerCharacter->GetName() : TEXT("NULL"));
            }

            CachedAIController = AIController;
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("AI setup complete - starting target planning"));
            UE_LOG(LogTemp, Log, TEXT("AI setup complete, starting target updates"));

            GetWorldTimerManager().ClearTimer(TargetUpdateTimerHandle); // Clear existing timer
            GetWorldTimerManager().SetTimer(TargetUpdateTimerHandle, this, &AFPSProjectGameModeBase::UpdateBotTarget, 2.0f, true);
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("ERROR: AIController null"));
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn AIController"));
        }
    }
    else
    {
        FString ErrorMsg = FString::Printf(TEXT("Player not ready - PC: %s, Pawn: %s, Character: %s"),
            PC ? TEXT("Valid") : TEXT("NULL"),
            PlayerPawn ? TEXT("Valid") : TEXT("NULL"),
            PlayerCharacter ? TEXT("Valid") : TEXT("NULL"));
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, ErrorMsg);
        UE_LOG(LogTemp, Warning, TEXT("%s, retrying..."), *ErrorMsg);

        // Try again if not ready yet
        FTimerHandle RetryHandle;
        GetWorldTimerManager().SetTimer(RetryHandle, this, &AFPSProjectGameModeBase::SetupPlayerAI, 0.5f, false);
    }
}

void AFPSProjectGameModeBase::UpdateBotTarget()
{
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("UPDATE CALLED"));

    // Use cached player character instead of GetPlayerPawn
    if (!CachedPlayerCharacter || !IsValid(CachedPlayerCharacter))
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("CachedPlayerCharacter is null or invalid!"));
        return;
    }

    if (!CachedAIController)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("CachedAIController is null!"));
        return;
    }

    if (!BotTargetPlanner)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("BotTargetPlanner is null!"));
        return;
    }

    FString MapName = GetWorld()->GetMapName();
    AActor* Target = nullptr;

    if (MapName.Contains(TEXT("Gym")))
    {
        TArray<AActor*> Collectibles;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACollectiblePickup::StaticClass(), Collectibles);

        for (AActor* Actor : Collectibles)
        {
            ACollectiblePickup* Pickup = Cast<ACollectiblePickup>(Actor);
            if (Pickup && Pickup->CollectibleType == ECollectibleType::FinishToken)
            {
                Target = Pickup;
                break;
            }
        }
    }
    else
    {
        BotTargetPlanner->EvaluateBestTarget(CachedAIController, CachedPlayerCharacter, Target);
    }

    if (Target)
    {
        CachedAIController->SetTarget(Target->GetActorLocation());
        FString DebugMsg = FString::Printf(TEXT("Target set to: %s"), *Target->GetName());
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, DebugMsg);
    }
}