// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include <Tests/AutomationCommon.h>

#define PERFORMANCE_TEST_MAP TEXT("/Game/PerformanceTests/PerformanceTestMap")

constexpr float MIN_EXPECTED_FPS = 20.0f;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerformanceTestSuite, "Game.Performance.PerformanceSuite", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

struct FWaitForPerformanceTestSuiteCompletion : public IAutomationLatentCommand {
	FPerformanceTestSuite* Test;
	FString CSVPath;
	float TimeoutSeconds;
	float Elapsed;
	FWaitForPerformanceTestSuiteCompletion(FPerformanceTestSuite* InTest, const FString& InCSVPath, float InTimeout = 1200.0f) : Test(InTest), CSVPath(InCSVPath), TimeoutSeconds(InTimeout), Elapsed(0.f) {}

	virtual bool Update() override {
		Elapsed += FApp::GetDeltaTime();

		if (IFileManager::Get().FileExists(*CSVPath)) {
			FString FileContents;
			if (FFileHelper::LoadFileToString(FileContents, *CSVPath)) {
				TArray<FString> Lines;
				FileContents.ParseIntoArrayLines(Lines);
				int32 ExpectedRows = -1;

				if (Lines.Num() > 0 && Lines[0].StartsWith(TEXT("#Tests:"))) {
					ExpectedRows = FCString::Atoi(*Lines[0].RightChop(7));
				}

				if (ExpectedRows < 1) {
					ExpectedRows = 8;
				}

				int32 DataRowStart = 2;
				int32 DataRows = Lines.Num() - DataRowStart;
				if (DataRows >= ExpectedRows) {
					bool bAllPassed = true;
					for (int32 i = DataRowStart; i < Lines.Num(); ++i) {
						TArray<FString> Columns;
						Lines[i].ParseIntoArray(Columns, TEXT(","), true);

						if (Columns.Num() < 5)
							continue;

						FString TestName = Columns[0].TrimStartAndEnd();
						float MinFPS = FCString::Atof(*Columns[1]);
						float MaxFPS = FCString::Atof(*Columns[2]);
						float AvgFPS = FCString::Atof(*Columns[3]);

						if (AvgFPS < MIN_EXPECTED_FPS) {
							Test->AddError(FString::Printf(TEXT("Test: %s failed: AvgFPS %.2f < Minimum %.2f"), *TestName, AvgFPS, MIN_EXPECTED_FPS));
							bAllPassed = false;
						}
						else {
							Test->AddInfo(FString::Printf(TEXT("Test: %s passed: AvgFPS %.2f >= Minimum %.2f"), *TestName, AvgFPS, MIN_EXPECTED_FPS));
						}
					}
					if (bAllPassed) {
						Test->AddInfo(TEXT("All performance tests passed"));
					}
					else {
						Test->AddError(TEXT("One or more performance tests failed"));
					}

					FString ArchiveDir = FPaths::ProjectSavedDir() / TEXT("Automation/Performance/");
					IFileManager::Get().MakeDirectory(*ArchiveDir, /*Tree=*/true);
					FString ArchivePath = ArchiveDir + "PerformanceTestResults_" +
						FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + ".csv";
					IFileManager::Get().Copy(*ArchivePath, *CSVPath);

					return true;
				}
			}
			else {
				Test->AddError("Could not read performance result CSV");
				return true;
			}
		}
		if (Elapsed > TimeoutSeconds) {
			Test->AddError(TEXT("Timeout waiting for performanc results CSV"));
			return true;
		}
		return false;
	}
};

bool FPerformanceTestSuite::RunTest(const FString& Parameters) {
	FString CSVPath = FPaths::ProjectDir() + TEXT("PerformanceTestResults_Automation.csv");
	if (IFileManager::Get().FileExists(*CSVPath)) {
		IFileManager::Get().Delete(*CSVPath);
	}

	AutomationOpenMap(PERFORMANCE_TEST_MAP);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPerformanceTestSuiteCompletion(this, CSVPath, 1200.0f));
	return true;
}