// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"

class AFPSCharacter;

#include "FPSHUD.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AFPSHUD : public AHUD
{
	GENERATED_BODY()


protected:
	UPROPERTY(EditDefaultsOnly)
	UTexture2D* CrosshairTexture;

public:
	virtual void DrawHUD() override;
	void ShowAchievement(const FString& Message);

	UPROPERTY(BlueprintReadOnly)
	FString LastDrawnAmmoString;

protected:
	FString AchievementMessage;
	float AchievementTimer = 0.0f;
};
