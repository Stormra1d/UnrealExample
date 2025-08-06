// Fill out your copyright notice in the Description page of Project Settings.


#include "FPS_SmokeFunctionalTest.h"
#include "Engine/World.h"
#include "../FPSCharacter.h"
#include "../FPSWeaponBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AFPS_SmokeFunctionalTest::AFPS_SmokeFunctionalTest()
	: TestPlayer(nullptr), TestWeapon(nullptr) {
	Rename(TEXT("Smoke Functional Test"));
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

	TestWeapon = GetWorld()->SpawnActor<AFPSWeaponBase>(AFPSWeaponBase::StaticClass());
	if (!TestWeapon) {
		FinishTest(EFunctionalTestResult::Failed, TEXT("Weapon could not be spawned"));
		return;
	}

	TestPlayer->SetPrimaryWeapon(TestWeapon);
	TestPlayer->AddMovementInput(FVector::ForwardVector, 1.f);
	TestPlayer->AddMovementInput(FVector::RightVector, 1.f);

	TestPlayer->StartFire();
	TestPlayer->Reload();

	const float InitialHealth = TestPlayer->GetCurrentHealth();
	TestPlayer->ApplyDamage(InitialHealth + 10.0f);

	FTimerDelegate TimerDel;
	TimerDel.BindLambda([this]() {
		if (!IsValid(TestPlayer) || TestPlayer->GetCurrentHealth() <= 0.0f) {
			FinishTest(EFunctionalTestResult::Succeeded, TEXT("Smoke Test Passed"));
		}
		else {
			FinishTest(EFunctionalTestResult::Failed, TEXT("Player did not die"));
		}

		if (TestPlayer && IsValid(TestPlayer)) {
			TestPlayer->Destroy();
			TestPlayer = nullptr;
		}
		if (TestWeapon && IsValid(TestWeapon)) {
			TestWeapon->Destroy();
			TestWeapon = nullptr;
		}

		});
	GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDel);
}