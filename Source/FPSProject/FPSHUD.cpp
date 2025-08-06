// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSHUD.h"
#include "FPSWeaponBase.h"
#include "FPSCharacter.h"

void AFPSHUD::DrawHUD() {
	Super::DrawHUD();

	// Crosshair
	if (CrosshairTexture) {
		FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

		FVector2D CrossHairDrawPosition(Center.X - (CrosshairTexture->GetSurfaceWidth() * 0.5f), Center.Y - (CrosshairTexture->GetSurfaceHeight() * 0.5f));

		FCanvasTileItem TileItem(CrossHairDrawPosition, CrosshairTexture->GetResource(), FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TileItem);
	}

	// Health
	AFPSCharacter* Player = Cast<AFPSCharacter>(GetOwningPawn());
	if (Player) {
		FString HealthStr = FString::Printf(TEXT("HP: %.0f"), Player->GetCurrentHealth());
		FVector2D HealthPos(1300.f, 50.f);
		FCanvasTextItem TextItem(HealthPos, FText::FromString(HealthStr), GEngine->GetLargeFont(), FLinearColor::White);
		TextItem.Scale = FVector2D(3.0f, 3.0f);
		Canvas->DrawItem(TextItem);
	}

	//Ammo
	if (Player) {
		if (Player->GetEquippedWeapon()) {
			int32 CurrentBullets = Player->GetEquippedWeapon()->BulletsInMag;
			int32 ReserveBullets = Player->GetEquippedWeapon()->GetTotalReserveBullets();
			LastDrawnAmmoString = FString::Printf(TEXT("%d / %d"), CurrentBullets, ReserveBullets);
			Canvas->DrawText(GEngine->GetLargeFont(), LastDrawnAmmoString, 50.f, 80.f);
		}
	}

	//Inventory
	if (Player) {
		FString InvString = FString::Printf(TEXT("Red Ruby: %d     |     Blue Sapphire: %d"), Player->RedRubyCount, Player->BlueSapphireCount);
		float X = Canvas->ClipX * 0.5f - 150.0f;
		float Y = Canvas->ClipY - 120.0f; 
		FVector2D TextPos(X, Y);
		FCanvasTextItem TextItem(TextPos, FText::FromString(InvString), GEngine->GetLargeFont(), FLinearColor::White);
		TextItem.Scale = FVector2D(2.f, 2.f);
		Canvas->DrawItem(TextItem);
	}

	//Achievement
	if (AchievementTimer > 0.0f) {
		float X = Canvas->ClipX * 0.5f - 200.f;
		float Y = Canvas->ClipY * 0.25f;
		FCanvasTextItem TextItem(FVector2D(X, Y),
			FText::FromString("Achievement: " + AchievementMessage),
			GEngine->GetLargeFont(),
			FLinearColor::Yellow);
		TextItem.Scale = FVector2D(2.0f, 2.0f);
		Canvas->DrawItem(TextItem);

		AchievementTimer -= GetWorld()->GetDeltaSeconds();
		if (AchievementTimer < 0.f) AchievementTimer = 0.f;
	}
}

void AFPSHUD::ShowAchievement(const FString& Message) {
	AchievementMessage = Message;
	AchievementTimer = 2.0f;
}