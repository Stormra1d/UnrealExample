// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSEnemyBase.h"
#include "FPSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "FPSEnemyPatrol.h"
#include "FPSEnemyDumb.h"
#include "FPSEnemySpawnManager.h"
#include <Kismet/GameplayStatics.h>

// Sets default values
AFPSEnemyBase::AFPSEnemyBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFPSEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;

	if (GetMesh()) {
		UMaterialInterface* BaseMaterial = GetMesh()->GetMaterial(0);
		if (BaseMaterial) {
			DynamicMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			GetMesh()->SetMaterial(0, DynamicMaterialInstance);
		}
	}
	
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AFPSEnemyBase::OnOverlapBegin);
}

// Called every frame
void AFPSEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AFPSEnemyBase::ReceiveDamage(float Amount) {
	CurrentHealth -= Amount;

	if (bFlashOnHit)
		StartHitFlash();

	if (CurrentHealth <= 0.f)
		OnDeath();
}

void AFPSEnemyBase::OnDeath() {
	if (OwningSpawner) {
		if (IsA(AFPSEnemyPatrol::StaticClass())) {
			OwningSpawner->NotifySmartEnemyDeath(Cast<AFPSEnemyPatrol>(this));
		}
		else if (IsA(AFPSEnemyDumb::StaticClass())) {
			OwningSpawner->NotifyDumbEnemyDeath(Cast<AFPSEnemyDumb>(this));
		}
	}

	AFPSCharacter* Player = Cast<AFPSCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (Player) {
		Player->OnEnemyKilled();
	}


	Destroy();
}

void AFPSEnemyBase::ApplyKnockback(const FVector& Direction, float Strenght) {
	LaunchCharacter(Direction * Strenght, true, true);
}

void AFPSEnemyBase::StartHitFlash() {
	if (DynamicMaterialInstance) {
		FLinearColor CurrentColor;
		FMaterialParameterInfo ParamInfo("BaseColor");

		FString MatName = DynamicMaterialInstance->GetName();

		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow, FString::Printf(TEXT("Dynamic Material Instance: %s"), *MatName));

		DynamicMaterialInstance->SetVectorParameterValue("BaseColor", FLinearColor::Red);
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("SetVectorParameterValue called"));
		DynamicMaterialInstance->GetVectorParameterValue(ParamInfo, CurrentColor);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, FString::Printf(TEXT("Current BaseColor: %s"), *CurrentColor.ToString()));

		GetWorld()->GetTimerManager().SetTimer(FlashTimerHandle, this, &AFPSEnemyBase::EndHitFlash, 0.2f, false);
	}
}

void AFPSEnemyBase::EndHitFlash() {
	if (DynamicMaterialInstance) {
		DynamicMaterialInstance->SetVectorParameterValue("BaseColor", FLinearColor::White);
	}
}

void AFPSEnemyBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	AFPSCharacter* Player = Cast<AFPSCharacter>(OtherActor);
	if (Player) {
		Player->ApplyDamage(ContactDamage);

		FVector KnockbackDirection = Player->GetActorLocation() - GetActorLocation();
		KnockbackDirection.Z = 0;
		KnockbackDirection.Normalize();

		float KnockbackStrength = 800.f;
		Player->LaunchCharacter(KnockbackDirection * KnockbackStrength + FVector(0, 0, 300.f), true, true);
	}
}