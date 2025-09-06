// Fill out your copyright notice in the Description page of Project Settings.

#include "FPS_SmokeFunctionalTest.h"
#include "Engine/World.h"
#include "../FPSCharacter.h"
#include "../FPSWeaponBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/AutomationTest.h"

AFPS_SmokeFunctionalTest::AFPS_SmokeFunctionalTest()
	: TestPlayer(nullptr), TestWeapon(nullptr), TestController(nullptr) {
	Rename(TEXT("SmokeFunctionalTest"));
	Description = "Testing";
}

void AFPS_SmokeFunctionalTest::StartTest() {
	Super::StartTest();

	FVector SpawnLocation(0, 0, 200);
	FRotator SpawnRotation = FRotator::ZeroRotator;

	TestPlayer = GetWorld()->SpawnActor<AFPSCharacter>(AFPSCharacter::StaticClass(), SpawnLocation, SpawnRotation);
	if (!TestPlayer) {
		FinishTest(EFunctionalTestResult::Failed, TEXT("Player could not be spawned"));
		return;
	}
	TestPlayer->bIsTestMode = true;

	TestController = GetWorld()->SpawnActor<APlayerController>();
	if (!TestController) {
		FinishTest(EFunctionalTestResult::Failed, TEXT("Controller could not be spawned"));
		return;
	}
	TestController->Possess(TestPlayer);

	TestWeapon = GetWorld()->SpawnActor<AFPSWeaponBase>(AFPSWeaponBase::StaticClass());
	if (!TestWeapon) {
		FinishTest(EFunctionalTestResult::Failed, TEXT("Weapon could not be spawned"));
		return;
	}
	TestPlayer->SetPrimaryWeapon(TestWeapon);

	AssertTrue(TestPlayer->GetPrimaryWeapon() == TestWeapon, TEXT("Primary Weapon should be set to TestWeapon"));

	FVector InitialLocation = TestPlayer->GetActorLocation();
	TestPlayer->AddMovementInput(FVector::ForwardVector, 1.f);
	TestPlayer->AddMovementInput(FVector::RightVector, 1.f);

	int32 InitialAmmo = TestWeapon->BulletsInMag;
	TestPlayer->StartFire();

	FTimerHandle FireHandle;
	GetWorld()->GetTimerManager().SetTimer(FireHandle, [this]()
		{
			TestPlayer->StopFire();
			TestPlayer->Reload();
		}, 0.0f, false);

	AssertTrue(TestWeapon->BulletsInMag < InitialAmmo, TEXT("Firing reduced ammo"));

	const float InitialHealth = TestPlayer->GetCurrentHealth();
	TestPlayer->ApplyDamage(InitialHealth + 10.0f);

	FTimerHandle CheckHandle;
	FTimerDelegate TimerDel;
	TimerDel.BindLambda([this, InitialLocation]() {
		FVector NewLocation = TestPlayer->GetActorLocation();
		AssertTrue(FVector::Dist(InitialLocation, NewLocation) > 0.f, TEXT("Movement applied"));

		if (!IsValid(TestPlayer) || TestPlayer->GetCurrentHealth() <= 0.0f) {
			FinishTest(EFunctionalTestResult::Succeeded, TEXT("Smoke Test Passed"));
		}
		else {
			FinishTest(EFunctionalTestResult::Failed, TEXT("Player did not die"));
		}
		CleanUp();
		});
	GetWorld()->GetTimerManager().SetTimer(CheckHandle, TimerDel, 0.0f, false);
}

void AFPS_SmokeFunctionalTest::CleanUp() {
	if (TestPlayer && IsValid(TestPlayer)) {
		TestPlayer->Destroy();
		TestPlayer = nullptr;
	}
	if (TestWeapon && IsValid(TestWeapon)) {
		TestWeapon->Destroy();
		TestWeapon = nullptr;
	}
	if (TestController && IsValid(TestController)) {
		TestController->Destroy();
		TestController = nullptr;
	}
}
