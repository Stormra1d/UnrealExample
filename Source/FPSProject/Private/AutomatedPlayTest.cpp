// Fill out your copyright notice in the Description page of Project Settings.

#include "AutomatedPlayTest.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "BotTestMonitorSubsystem.h"
#include "Tests/AutomationCommon.h"
#include <FunctionalTest.h>
#include "../FPSProjectGameModeBase.h"
#include "PlayerAIController.h"
#include "../FPSCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutomatedPlayTest, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomatedPlayTest, "Game.Automation.FPSCharacter.AutomatedPlaytest", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

class FWaitForBotMonitorLatentCommand : public IAutomationLatentCommand {
public:
    FWaitForBotMonitorLatentCommand(FAutomatedPlayTest* InTest)
        : Test(InTest), Elapsed(0.0f), Timeout(180.f) {
    }

    virtual bool Update() override {
        UWorld* World = GWorld;
        if (!World) { Test->AddError(TEXT("Could not get valid game world")); return true; }

        if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE) {
            Test->AddError(TEXT("World is not a game or PIE world"));
            return true;
        }

        UGameInstance* GI = World->GetGameInstance();
        if (!GI) { Test->AddError(TEXT("Could not get GameInstance")); return true; }
        UBotTestMonitorSubsystem* Monitor = GI->GetSubsystem<UBotTestMonitorSubsystem>();
        if (Monitor && Monitor->IsTestFinished()) {
            if (Monitor->TestResult == EBotTestOutcome::Completed) {
                Test->TestTrue(TEXT("Bot finished successfully"), true);
            }
            else {
                FString Reason = FString::Printf(TEXT("Bot test failed. Outcome: %d"), static_cast<int32>(Monitor->TestResult));
                Test->AddError(*Reason);
                Test->TestTrue(TEXT("Bot test did not finish successfully"), false);
            }
            return true;
        }
        Elapsed += FApp::GetDeltaTime();
        if (Elapsed > Timeout) {
            Test->AddError(TEXT("Timed out waiting for BotTestMonitor to finish"));
            return true;
        }
        return false;
    }
private:
    FAutomatedPlayTest* Test;
    float Elapsed;
    float Timeout;
};

class FWaitForPlayerLatentCommand : public IAutomationLatentCommand {
public:
    FWaitForPlayerLatentCommand(FAutomatedPlayTest* InTest) : Test(InTest), Elapsed(0.0f), MaxWaitTime(30.0f) {}

    virtual bool Update() override {
        UWorld* World = GWorld;
        if (!World) {
            Test->AddError(TEXT("Could not get valid game world for player wait"));
            return true;
        }

        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
        APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
        AFPSCharacter* PlayerCharacter = Cast<AFPSCharacter>(PlayerPawn);

        if (!PC || !PlayerPawn || !PlayerCharacter) {
            Elapsed += FApp::GetDeltaTime();
            if (Elapsed > MaxWaitTime) {
                Test->AddError(TEXT("Timed out waiting for player controller/pawn to initialize"));
                return true;
            }
            UE_LOG(LogAutomatedPlayTest, Warning, TEXT("Player not ready: PC=%s, Pawn=%s, Character=%s"),
                PC ? TEXT("Valid") : TEXT("NULL"),
                PlayerPawn ? TEXT("Valid") : TEXT("NULL"),
                PlayerCharacter ? TEXT("Valid") : TEXT("NULL"));
            return false; // Keep waiting
        }

        UE_LOG(LogAutomatedPlayTest, Log, TEXT("Player fully initialized: %s"), *PlayerCharacter->GetName());
        return true;
    }

private:
    FAutomatedPlayTest* Test;
    float Elapsed;
    float MaxWaitTime;
};

class FWaitForPlayerAIReadyLatentCommand : public IAutomationLatentCommand {
public:
    FWaitForPlayerAIReadyLatentCommand(FAutomatedPlayTest* InTest, float InTimeout = 20.0f)
        : Test(InTest), Timeout(InTimeout), Elapsed(0.0f) {
    }

    virtual bool Update() override {
        UWorld* World = GWorld;
        if (!World) {
            Test->AddError(TEXT("Could not get valid game world"));
            return true;
        }

        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        AFPSCharacter* Character = Cast<AFPSCharacter>(Pawn);

        // Wait for AI controller possession
        if (Character && Character->Controller && Character->Controller->IsA<APlayerAIController>()) {
            UE_LOG(LogAutomatedPlayTest, Log, TEXT("AI controller possessed player: %s"), *Character->GetName());
            return true;
        }

        Elapsed += FApp::GetDeltaTime();
        if (Elapsed > Timeout) {
            Test->AddError(TEXT("Timed out waiting for AI controller to possess player"));
            return true;
        }
        return false; // Keep waiting
    }
private:
    FAutomatedPlayTest* Test;
    float Timeout;
    float Elapsed;
};


bool FAutomatedPlayTest::RunTest(const FString& Parameters) {
    if (!FParse::Param(FCommandLine::Get(), TEXT("AITest"))) {
        UE_LOG(LogAutomatedPlayTest, Error, TEXT("Test requires AITest argument"));
        return false;
    }

    const FString MapPath = TEXT("/Game/Maps/Gym");
    if (!AutomationOpenMap(MapPath)) {
        UE_LOG(LogAutomatedPlayTest, Error, TEXT("Failed to load map %s"), *MapPath);
        return false;
    }

    // Wait for level to open
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerLatentCommand(this));

    // Setup AI after map loads
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerAIReadyLatentCommand(this));

    // Wait for AI setup and then monitor
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForBotMonitorLatentCommand(this));

    return true;
}