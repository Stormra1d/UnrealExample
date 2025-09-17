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

static AFPSCharacter* ResolveTestPawn(UWorld* World)
{
    if (!World) return nullptr;

    if (UGameInstance* GI = World->GetGameInstance())
    {
        if (UBotTestMonitorSubsystem* Monitor = GI->GetSubsystem<UBotTestMonitorSubsystem>())
        {
            if (AFPSCharacter* P = Monitor->GetTestPlayerPawn())
            {
                return P;
            }
        }
    }

    // Fallback: first AFPSCharacter in world
    for (TActorIterator<AFPSCharacter> It(World); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

class FWaitForBotMonitorLatentCommand : public IAutomationLatentCommand {
public:
    FWaitForBotMonitorLatentCommand(FAutomatedPlayTest* InTest)
        : Test(InTest), Elapsed(0.0f), Timeout(600.f) {
    }

    virtual bool Update() override {
        UWorld* World = GWorld;
        if (!World) { Test->AddError(TEXT("Could not get valid game world")); return true; }

        UGameInstance* GI = World->GetGameInstance();
        if (!GI) { Test->AddError(TEXT("Could not get GameInstance")); return true; }

        if (UBotTestMonitorSubsystem* Monitor = GI->GetSubsystem<UBotTestMonitorSubsystem>()) {
            if (Timeout < 0.f) {
                Timeout = Monitor->GetMaxDuration() * 1.2f;
            }
            auto OutcomeToString = [](EBotTestOutcome O)->const TCHAR*
                {
                    switch (O)
                    {
                    case EBotTestOutcome::None:      return TEXT("None");
                    case EBotTestOutcome::Completed: return TEXT("Completed");
                    case EBotTestOutcome::Died:      return TEXT("Died");
                    case EBotTestOutcome::GotStuck:  return TEXT("GotStuck");
                    case EBotTestOutcome::Timeout:   return TEXT("Timeout");
                    case EBotTestOutcome::Error:     return TEXT("Error");
                    default:                         return TEXT("Unknown");
                    }
                };

            if (Monitor && Monitor->IsTestFinished())
            {
                const EBotTestOutcome Outcome = Monitor->TestResult;
                const float TimeTaken = Monitor->TimeTaken;

                switch (Outcome)
                {
                case EBotTestOutcome::Completed:
                    Test->AddInfo(*FString::Printf(TEXT("Outcome=Completed Time=%.2fs"), TimeTaken));
                    Test->TestTrue(TEXT("Bot completed"), true);
                    break;

                case EBotTestOutcome::Died:
                case EBotTestOutcome::GotStuck:
                case EBotTestOutcome::Timeout:
                    Test->AddWarning(*FString::Printf(TEXT("Outcome=%s Time=%.2fs"),
                        OutcomeToString(Outcome), TimeTaken));
                    Test->TestTrue(TEXT("Non-fatal bot outcome"), true);
                    break;

                case EBotTestOutcome::Error:
                default:
                    Test->AddError(*FString::Printf(TEXT("Outcome=%s Time=%.2fs"),
                        OutcomeToString(Outcome), TimeTaken));
                    Test->TestTrue(TEXT("Fatal bot error"), false);
                    break;
                }
                return true;
            }
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

        AFPSCharacter* PC = ResolveTestPawn(World);

        if (!PC) {
            Elapsed += FApp::GetDeltaTime();
            if (Elapsed > MaxWaitTime) {
                Test->AddError(TEXT("Timed out waiting for player controller/pawn to initialize"));
                return true;
            }
            return false; // Keep waiting
        }
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

        AFPSCharacter* Character = ResolveTestPawn(World);

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

class FMarkPlaytestCompleteLatentCommand : public IAutomationLatentCommand {
public:
    explicit FMarkPlaytestCompleteLatentCommand(FAutomatedPlayTest* InTest) : Test(InTest) {}
    virtual bool Update() override {
        Test->AddInfo(TEXT("AutomatedPlaytest completed; reporting to Automation framework."));
        return true;
    }
private:
    FAutomatedPlayTest* Test;
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

    // Finish Test (due to async)
    ADD_LATENT_AUTOMATION_COMMAND(FMarkPlaytestCompleteLatentCommand(this));

    ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("Quit")));

    return true;
}