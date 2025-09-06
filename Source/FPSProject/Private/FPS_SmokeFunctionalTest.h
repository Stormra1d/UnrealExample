// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "FPS_SmokeFunctionalTest.generated.h"

UCLASS()
class AFPS_SmokeFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AFPS_SmokeFunctionalTest();

	virtual void StartTest() override;
	void CleanUp();

private:
	class AFPSCharacter* TestPlayer = nullptr;
	class AFPSWeaponBase* TestWeapon = nullptr;
	class APlayerController* TestController = nullptr;
};
#endif