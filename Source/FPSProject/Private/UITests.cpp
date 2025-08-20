#if WITH_EDITOR

#include "../FPSProject.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "IAutomationDriverModule.h"
#include "IAutomationDriver.h"
#include "IDriverSequence.h"
#include "IDriverElement.h"
#include "LocateBy.h"
#include "Async/Async.h"
#include "HAL/Event.h"
#include "Editor.h"
#include "UnrealEd.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Settings/LevelEditorPlaySettings.h"

static constexpr const TCHAR* MapPath = TEXT("/Game/Maps/Gym");
static constexpr const TCHAR* MainMenuName = TEXT("MainMenu");

static void StartPIE_GtOnly()
{
    if (GEditor && GEditor->PlayWorld) return;
    FRequestPlaySessionParams Params;
    Params.WorldType = EPlaySessionWorldType::PlayInEditor;
    GEditor->RequestPlaySession(Params);
}

static void StopPIE_GtOnly()
{
    if (GEditor && GEditor->PlayWorld)
    {
        GEditor->RequestEndPlayMap();
    }
}

static bool IsPIEReady()
{
    if (!GEditor || !GEditor->PlayWorld) return false;
    const UWorld* W = GEditor->PlayWorld;
    return W && W->HasBegunPlay() && W->GetFirstPlayerController() != nullptr;
}

static void RunOnGTBlocking(TFunction<void()> Fn)
{
    if (IsInGameThread())
    {
        Fn();
        return;
    }
    FEventRef Done;
    AsyncTask(ENamedThreads::GameThread, [F = MoveTemp(Fn), &Done]()
        {
            F();
            Done->Trigger();
        });
    Done->Wait();
}

static void FocusPIEViewport_GtOnly()
{
    if (!GEditor || !GEditor->PlayWorld) return;
    if (UGameViewportClient* GVC = GEditor->PlayWorld->GetGameViewport())
    {
        TSharedPtr<SViewport> SVP = GVC->GetGameViewportWidget();
        if (SVP.IsValid())
        {
            FSlateApplication::Get().SetAllUserFocus(SVP, EFocusCause::SetDirectly);
        }
    }
}

static bool DriverSeesRoot(FAutomationDriverPtr& Driver, TFunction<void(const FString&)> Log)
{
    auto RootById = Driver->FindElement(By::Id(TEXT("PauseMenuRoot")));
    auto RootByPath = Driver->FindElement(By::Path(TEXT("#PauseMenuRoot")));
    Log(FString::Printf(
        TEXT("Root: By::Id exists=%d vis=%d | By::Path exists=%d vis=%d"),
        RootById->Exists() ? 1 : 0, RootById->IsVisible() ? 1 : 0,
        RootByPath->Exists() ? 1 : 0, RootByPath->IsVisible() ? 1 : 0));
    return (RootById->Exists() && RootById->IsVisible()) ||
        (RootByPath->Exists() && RootByPath->IsVisible());
}

static FDriverElementRef FindResumeInDriver(FAutomationDriverPtr& Driver, TFunction<void(const FString&)> Log)
{
    auto ById = Driver->FindElement(By::Id(TEXT("PauseMenu_ResumeBtn")));
    auto ByPath = Driver->FindElement(By::Path(TEXT("#PauseMenuRoot #PauseMenu_ResumeBtn")));
    Log(FString::Printf(
        TEXT("ResumeBtn: By::Id exists=%d vis=%d | By::Path exists=%d vis=%d"),
        ById->Exists() ? 1 : 0, ById->IsVisible() ? 1 : 0,
        ByPath->Exists() ? 1 : 0, ByPath->IsVisible() ? 1 : 0));
    if (ById->Exists()) return ById;
    if (ByPath->Exists()) return ByPath;
    return ById;
}

BEGIN_DEFINE_SPEC(FUIPauseMenuSpec, "Game.FPSCharacter.UI.Pause",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
END_DEFINE_SPEC(FUIPauseMenuSpec)

void FUIPauseMenuSpec::Define()
{
    BeforeEach([this]()
        {
            AutomationOpenMap(MapPath);
            RunOnGTBlocking([] { StartPIE_GtOnly(); });
            const double Deadline = FPlatformTime::Seconds() + 20.0;
            while (!IsPIEReady() && FPlatformTime::Seconds() < Deadline)
            {
                FPlatformProcess::Sleep(0.05);
            }
            TestTrue("PIE did not become ready within 20s", IsPIEReady());
            RunOnGTBlocking([this]
                {
                    if (!IAutomationDriverModule::Get().IsEnabled())
                    {
                        IAutomationDriverModule::Get().Enable();
                    }
                });
            Driver = IAutomationDriverModule::Get().CreateDriver();
            AddInfo(TEXT("Driver created"));
        });

    AfterEach([this]()
        {
            if (Driver.IsValid()) Driver.Reset();
            RunOnGTBlocking([this]
                {
                    if (IAutomationDriverModule::Get().IsEnabled())
                    {
                        IAutomationDriverModule::Get().Disable();
                    }
                });
            RunOnGTBlocking([] { StopPIE_GtOnly(); });
        });

    It("OpensWithP", EAsyncExecution::ThreadPool, [this]()
        {
            TestTrue("PIE not ready", IsPIEReady());
            auto FocusLambda = [] { FocusPIEViewport_GtOnly(); };
            RunOnGTBlocking(FocusLambda);
            auto Seq = Driver->CreateSequence();
            Seq->Actions()
                .Press(FKey("P"))
                .Wait(FTimespan::FromMilliseconds(50))
                .Release(FKey("P"));
            Seq->Perform();
            Driver->Wait(FTimespan::FromSeconds(0.5));
            auto Log = [this](const FString& Msg) { AddInfo(Msg); };
            auto RootById = Driver->FindElement(By::Id(TEXT("PauseMenuRoot")));
            bool bVisible = RootById->Exists() && RootById->IsVisible();
            TestTrue("PauseMenuRoot should be visible after pressing P", bVisible);
            if (!bVisible)
            {
                auto RootByPath = Driver->FindElement(By::Path(TEXT("#PauseMenuRoot")));
            }
        });

    It("ResumeButtonCloses", EAsyncExecution::ThreadPool, [this]()
        {
            TestTrue("PIE not ready", IsPIEReady());
            auto FocusLambda = [] { FocusPIEViewport_GtOnly(); };
            RunOnGTBlocking(FocusLambda);
            auto SeqOpen = Driver->CreateSequence();
            SeqOpen->Actions()
                .Press(FKey("P"))
                .Wait(FTimespan::FromMilliseconds(50))
                .Release(FKey("P"));
            SeqOpen->Perform();
            Driver->Wait(FTimespan::FromSeconds(1.0));
            auto Log = [this](const FString& Msg) { AddInfo(Msg); };
            auto RootById = Driver->FindElement(By::Id(TEXT("PauseMenuRoot")));
            bool bOpened = RootById->Exists() && RootById->IsVisible();
            TestTrue("Pause menu did not open", bOpened);
            auto ResumeBtn = FindResumeInDriver(Driver, Log);
            bool bButtonFound = ResumeBtn->Exists() && ResumeBtn->IsVisible();
            TestTrue("Resume button not found", bButtonFound);
            if (!bButtonFound)
            {
                AddInfo(TEXT("Skipping click because Resume button was not found"));
                return;
            }
            auto SeqClick = Driver->CreateSequence();
            SeqClick->Actions().Focus(ResumeBtn).Click(ResumeBtn);
            SeqClick->Perform();
            Driver->Wait(FTimespan::FromSeconds(1.0));
            RootById = Driver->FindElement(By::Id(TEXT("PauseMenuRoot")));
            bool bStillVisible = RootById->Exists() && RootById->IsVisible();
            TestFalse("PauseMenuRoot should be hidden after clicking Resume", bStillVisible);
        });

    It("OpensWithEscape", EAsyncExecution::ThreadPool, [this]()
        {
            TestTrue("PIE not ready", IsPIEReady());
            RunOnGTBlocking([] { FocusPIEViewport_GtOnly(); });

            auto Seq = Driver->CreateSequence();
            Seq->Actions()
                .Press(FKey("Escape"))
                .Wait(FTimespan::FromMilliseconds(50))
                .Release(FKey("Escape"));
            Seq->Perform();

            Driver->Wait(FTimespan::FromSeconds(0.5));

            auto Root = Driver->FindElement(By::Id(TEXT("PauseMenuRoot")));
            bool bVisible = Root->Exists() && Root->IsVisible();
            TestTrue("Pause menu should be visible after Escape", bVisible);
        });

    It("QuitToMainMenu", EAsyncExecution::ThreadPool, [this]()
        {
            TestTrue("PIE not ready", IsPIEReady());
            RunOnGTBlocking([] { FocusPIEViewport_GtOnly(); });

            auto Open = Driver->CreateSequence();
            Open->Actions()
                .Press(FKey("P"))
                .Wait(FTimespan::FromMilliseconds(50))
                .Release(FKey("P"));
            Open->Perform();

            Driver->Wait(FTimespan::FromSeconds(1.0));

            auto Root = Driver->FindElement(By::Id(TEXT("PauseMenuRoot")));
            TestTrue("Pause menu did not open", Root->Exists() && Root->IsVisible());

            auto QuitBtn = Driver->FindElement(By::Id(TEXT("PauseMenu_QuitBtn")));
            if (!QuitBtn->Exists())
                QuitBtn = Driver->FindElement(By::Path(TEXT("#PauseMenuRoot #PauseMenu_QuitBtn")));
            TestTrue("Quit button not found", QuitBtn->Exists() && QuitBtn->IsVisible());

            auto Click = Driver->CreateSequence();
            Click->Actions().Focus(QuitBtn).Click(QuitBtn);
            Click->Perform();

            const double Deadline = FPlatformTime::Seconds() + 15.0;
            bool bOnMainMenu = false;
            while (FPlatformTime::Seconds() < Deadline)
            {
                if (GEditor && GEditor->PlayWorld)
                {
                    const FString Map = GEditor->PlayWorld->GetMapName();
                    if (Map.Contains(MainMenuName))
                    {
                        bOnMainMenu = true;
                        break;
                    }
                }
                FPlatformProcess::Sleep(0.1);
            }

            TestTrue("Did not reach Main Menu after Quit", bOnMainMenu);
        });
}

#endif
