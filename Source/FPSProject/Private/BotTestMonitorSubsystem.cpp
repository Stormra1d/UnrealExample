#include "BotTestMonitorSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

void UBotTestMonitorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    bIsAIPlaytest = FParse::Param(FCommandLine::Get(), TEXT("AITest"));
    if (!bIsAIPlaytest) {
        UE_LOG(LogTemp, Warning, TEXT("Subsystem: Not an AI Playtest, disabling"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("UBotTestMonitorSubsystem::Initialize: bFinished=%s"), bFinished ? TEXT("true") : TEXT("false"))

    GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(TEXT("WE INITIALIZED THE SUBSYSTEM")));

    FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UBotTestMonitorSubsystem::OnWorldInitialized);

    FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UBotTestMonitorSubsystem::OnWorldTearDown);
}

void UBotTestMonitorSubsystem::Deinitialize()
{
    FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
    FWorldDelegates::OnWorldBeginTearDown.RemoveAll(this);

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    }
    Super::Deinitialize();
}

bool UBotTestMonitorSubsystem::Tick(float DeltaTime)
{
    if (!bIsAIPlaytest) return true;

    UE_LOG(LogTemp, Warning, TEXT("Tick start: bFinished=%s, bReady=%s, Elapsed=%.2f"), bFinished ? TEXT("true") : TEXT("false"), bReady ? TEXT("true") : TEXT("false"), Elapsed);

    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("TESTTICK"));

    if (bFinished) {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Test already finished"));
        return true;
    }

    Elapsed += DeltaTime;

    UWorld* World = GetWorld();
    if (!World) {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("WORLD IS NULL"));
        UE_LOG(LogTemp, Error, TEXT("World is null"));
        NotifyTestComplete(EBotTestOutcome::Error, Elapsed);
        return true;
    }
    GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(TEXT("WORLD VALID: %s"), *World->GetName()));
    UE_LOG(LogTemp, Log, TEXT("World valid: %s"), *World->GetName());

    AFPSCharacter* Player = TestPlayerPawn;
    if (!IsValid(Player)) {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("No TestPlayerPawn, using fallback"));
        UE_LOG(LogTemp, Warning, TEXT("TestPlayerPawn is null, attempting fallback"));
        Player = Cast<AFPSCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
        if (!Player) {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Fallback player is NULL"));
            UE_LOG(LogTemp, Warning, TEXT("Fallback player is null"));
            if (Elapsed > 10.0f) {
                GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Setup timeout - failing test"));
                NotifyTestComplete(EBotTestOutcome::Error, Elapsed);
                return true;
            }
            return false;
        }
    }

    if (!bReady) {
        if (Player && Player->Controller && Player->GetCurrentHealth() > 0.0f) {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("READY"));
            UE_LOG(LogTemp, Log, TEXT("Player ready: %s, Health=%.1f"), *Player->GetName(), Player->GetCurrentHealth());
            bReady = true;
        }
        else {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(TEXT("NOT READY - Player: %s, Controller: %s, Health: %.1f"),
                Player ? TEXT("Valid") : TEXT("NULL"),
                (Player && Player->Controller) ? TEXT("Valid") : TEXT("NULL"),
                Player ? Player->GetCurrentHealth() : -1.0f));
            UE_LOG(LogTemp, Warning, TEXT("Player not ready: Player=%s, Controller=%s, Health=%.1f"),
                Player ? *Player->GetName() : TEXT("NULL"),
                (Player && Player->Controller) ? *Player->Controller->GetName() : TEXT("NULL"),
                Player ? Player->GetCurrentHealth() : -1.0f);
            if (Elapsed > 10.0f) {
                GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Setup timeout - failing test"));
                NotifyTestComplete(EBotTestOutcome::Error, Elapsed);
                return true;
            }
            return false;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Checking timeout: Elapsed=%.2f, MaxDuration=%.2f"), Elapsed, MaxDuration);

    const FVector CurPos = Player->GetActorLocation();
    const FVector Delta = CurPos - LastPos;
    const float Speed = Player->GetVelocity().Size();

    if (Elapsed == 0.f) { 
        LastPos = CurPos; 
        LastMovementTime = 0.f; 
    }

    if (Elapsed > MaxDuration) {
        NotifyTestComplete(EBotTestOutcome::Timeout, Elapsed);
        return true;
    }

    bool bMoved = Delta.Size() > MovementEps || Speed > SpeedEps;
    if (bMoved) {
        LastMovementTime = Elapsed;
        LastPos = CurPos;
    }

    if ((Elapsed - LastMovementTime) > StuckTimeout) {
        NotifyTestComplete(EBotTestOutcome::GotStuck, Elapsed);
        return true;
    }

    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("TESTPLAYER"));
    UE_LOG(LogTemp, Log, TEXT("Checking player: %s"), *Player->GetName());

    if (!Player || Player->IsPendingKillPending()) {
        if (Elapsed > 5.0f) {
            NotifyTestComplete(EBotTestOutcome::Error, Elapsed);
            return true;
        }
        return false;
    }

    if (Player->GetCurrentHealth() <= 0) {
        if (Elapsed > 5.0f) {
            NotifyTestComplete(EBotTestOutcome::Died, Elapsed);
            return true;
        }
    }

    int32 TotalCollected = Player->RedRubyCount + Player->BlueSapphireCount;
    int32 KillCount = Player->KillCount;

    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("TESTCOLLECTED %d"), TotalCollected));
    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("TESTKILL %d"), KillCount));
    UE_LOG(LogTemp, Log, TEXT("Progress: Collected=%d, Kills=%d"), TotalCollected, KillCount);

    if (KillCount >= 20 && TotalCollected >= 10) {
        NotifyTestComplete(EBotTestOutcome::Completed, Elapsed);
        return true;
    }

    return true;
}


void UBotTestMonitorSubsystem::StartTest() {
    UE_LOG(LogTemp, Warning, TEXT("UBotTestMonitorSubsystem::StartTest: bFinished=%s (should be false after this)"), bFinished ? TEXT("true") : TEXT("false"));

    bIsBatchMode = FParse::Param(FCommandLine::Get(), TEXT("BatchBot"));
    bFinished = false;
    TestResult = EBotTestOutcome::None;
    Elapsed = 0.0f;
    MaxDuration = 500.0f;
    StartSeconds = FPlatformTime::Seconds();
    StartTimeStamp = FDateTime::UtcNow();
    CurrentRunName = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    CurrentReplayName = FString::Printf(TEXT("BotReplay_%s"), *CurrentRunName);
    StartReplay();
}

void UBotTestMonitorSubsystem::NotifyTestComplete(EBotTestOutcome Outcome, float TimeTakenParam) {
    if (bFinished) return;
    bFinished = true;

    TestResult = Outcome;
    TimeTaken = TimeTakenParam;

    StopReplay();

    if (bIsBatchMode) {
        AppendToBatchLog();
    }
    else {
        WriteSingleRunLog();
    }

    UE_LOG(LogTemp, Warning, TEXT("TEST OUTCOME: %d, TimeTaken: %.2f"), (int32)Outcome, TimeTaken);
    
    if (GEngine && bIsAIPlaytest)
    {
        if (!GIsAutomationTesting && !bIsBatchMode)
        {
            GEngine->Exec(GetWorld(), TEXT("quit"));
        }
    }
}

void UBotTestMonitorSubsystem::WriteSingleRunLog() {
    float FPS = 0.f, Mem = 0.f;
    CollectPerformanceMetrics(FPS, Mem);
    FBotTestRunData Run;
    Run.RunName = CurrentRunName;
    Run.Outcome = TestResult;
    Run.TimeTaken = TimeTaken;
    Run.AvgFPS = FPS;
    Run.MaxMemoryMB = Mem;
    Run.ReplayName = CurrentReplayName;
    FString OutputString;
    FJsonObjectConverter::UStructToJsonObjectString(Run, OutputString);
    FString FullPath = FPaths::ProjectSavedDir() / ResultLogPath;
    FFileHelper::SaveStringToFile(OutputString, *FullPath);
}

void UBotTestMonitorSubsystem::AppendToBatchLog() {
    float FPS = 0.f, Mem = 0.f;
    CollectPerformanceMetrics(FPS, Mem);
    FString FullPath = FPaths::ProjectSavedDir() / ResultLogPath;
    FString JsonRaw;
    FBotTestBatchResult BatchResult;
    if (FPaths::FileExists(FullPath) && FFileHelper::LoadFileToString(JsonRaw, *FullPath)) {
        FJsonObjectConverter::JsonObjectStringToUStruct(JsonRaw, &BatchResult, 0, 0);
    }
    FBotTestRunData NewRun;
    NewRun.RunName = CurrentRunName;
    NewRun.Outcome = TestResult;
    NewRun.TimeTaken = TimeTaken;
    NewRun.AvgFPS = FPS;
    NewRun.MaxMemoryMB = Mem;
    NewRun.ReplayName = CurrentReplayName;
    BatchResult.AllRuns.Add(NewRun);
    switch (TestResult) {
    case EBotTestOutcome::Completed: BatchResult.CompletedCount++; break;
    case EBotTestOutcome::Died: BatchResult.DiedCount++; break;
    case EBotTestOutcome::GotStuck: BatchResult.GotStuckCount++; break;
    case EBotTestOutcome::Timeout: BatchResult.TimeoutCount++; break;
    case EBotTestOutcome::Error: BatchResult.ErrorCount++; break;
    default: break;
    }
    float TotalTime = 0.0f, TotalFPS = 0.0f, MaxMem = 0.0f;
    for (const FBotTestRunData& Run : BatchResult.AllRuns) {
        TotalTime += Run.TimeTaken;
        TotalFPS += Run.AvgFPS;
        MaxMem = FMath::Max(MaxMem, Run.MaxMemoryMB);
    }
    int32 Count = BatchResult.AllRuns.Num();
    BatchResult.AvgTime = Count > 0 ? TotalTime / Count : 0.0f;
    BatchResult.AvgFPS = Count > 0 ? TotalFPS / Count : 0.0f;
    BatchResult.MaxMemoryPeak = MaxMem;
    FString Output;
    FJsonObjectConverter::UStructToJsonObjectString(BatchResult, Output);
    FFileHelper::SaveStringToFile(Output, *FullPath);
}

void UBotTestMonitorSubsystem::StartReplay() {
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance()) {
        World->GetGameInstance()->StartRecordingReplay(CurrentReplayName, TEXT("BotTestReplay"));
    }
}

void UBotTestMonitorSubsystem::StopReplay() {
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance()) {
        World->GetGameInstance()->StopRecordingReplay();
    }
}

void UBotTestMonitorSubsystem::CollectPerformanceMetrics(float& OutAvgFPS, float& OutMaxMemory) {
    if (GEngine && GEngine->GameViewport) {
        float Time = FPlatformTime::Seconds() - StartSeconds;
        if (Time > 0) {
            float FrameRate = 1.0f / FApp::GetDeltaTime();
            OutAvgFPS = FrameRate;
        }
    }
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    OutMaxMemory = MemStats.UsedPhysical / (1024.0f * 1024.0f);
}

void UBotTestMonitorSubsystem::OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS)
{
    if (World && World->IsGameWorld() && bIsAIPlaytest)
    {
        if (TickHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        }
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &UBotTestMonitorSubsystem::Tick)
        );

        StartTest();
    }
}

void UBotTestMonitorSubsystem::OnWorldTearDown(UWorld* World)
{
    if (World && World->IsGameWorld() && bIsAIPlaytest)
    {
        UE_LOG(LogTemp, Warning, TEXT("World tearing down. Invalidating TestPlayerPawn."));
        TestPlayerPawn = nullptr;
    }
}