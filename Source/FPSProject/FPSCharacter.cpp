// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "FPSWeaponBase.h"
#include "FPSHUD.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubSystems.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include <GameFramework/GameModeBase.h>

// Sets default values
AFPSCharacter::AFPSCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	FPSCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	check(FPSCameraComponent != nullptr);

	FPSCameraComponent->SetupAttachment(GetCapsuleComponent());
	FPSCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BaseEyeHeight));
	FPSCameraComponent->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	check(FPSMesh != nullptr);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetupAttachment(FPSCameraComponent);
	FPSMesh->bCastDynamicShadow = false;
	FPSMesh->CastShadow = false;

	GetMesh()->SetOwnerNoSee(true);

	thisEngineIsDumb = true;
}

// Called when the game starts or when spawned
void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;

	if (APlayerController* PC = Cast<APlayerController>(Controller)) {
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) {
			if (DefaultMappingContext) {
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("AFPSCharacter BeginPlay, CurrentHealth=%f"), CurrentHealth);

	//TESTING
	if (WeaponClassToSpawn) {
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		PrimaryWeapon = GetWorld()->SpawnActor<AFPSWeaponBase>(WeaponClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (PrimaryWeapon) {
			EquippedWeapon = PrimaryWeapon;
			EquippedWeapon->AttachToComponent(FPSCameraComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			EquippedWeapon->SetActorRelativeLocation(FVector(50.f, 20.f, -20.f));
			EquippedWeapon->SetActorRelativeRotation(FRotator(0.f, 0.f, 0.0f));
		}
	}

	DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	DefaultCameraOffset = FPSCameraComponent->GetRelativeLocation();
	TargetCameraOffset = DefaultCameraOffset;

	float HeightDifference = DefaultCapsuleHalfHeight - CrouchedHeight;
	CrouchedCameraOffset = FVector(DefaultCameraOffset.X, DefaultCameraOffset.Y, DefaultCameraOffset.Z - HeightDifference);
}

// Called every frame
void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Current = FPSCameraComponent->GetRelativeLocation();
	FVector NewLocation = FMath::VInterpTo(Current, TargetCameraOffset, DeltaTime, 8.f);
	FPSCameraComponent->SetRelativeLocation(NewLocation);
}

// Called to bind functionality to input
void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		if (IA_MoveForward) {
			EnhancedInput->BindAction(IA_MoveForward, ETriggerEvent::Triggered, this, &AFPSCharacter::MoveForward);
		}

		if (IA_MoveRight) {
			EnhancedInput->BindAction(IA_MoveRight, ETriggerEvent::Triggered, this, &AFPSCharacter::MoveRight);
		}

		if (IA_Look) {
			EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AFPSCharacter::Look);
		}

		if (IA_Jump) {
			EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &AFPSCharacter::StartJump);
			EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AFPSCharacter::StopJump);
		}

		if (IA_Fire) {
			EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AFPSCharacter::StartFire);
			EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AFPSCharacter::StopFire);
		}

		if (IA_Reload) {
			EnhancedInput->BindAction(IA_Reload, ETriggerEvent::Started, this, &AFPSCharacter::Reload);
		}

		if (IA_Sprint) {
			EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &AFPSCharacter::Sprint);
			EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AFPSCharacter::Sprint);
		}
		if (IA_Swap) {
			EnhancedInput->BindAction(IA_Swap, ETriggerEvent::Triggered , this, &AFPSCharacter::SwapWeapon);
		}

		if (IA_Pause) {
			EnhancedInput->BindAction(IA_Pause, ETriggerEvent::Triggered, this, &AFPSCharacter::HandlePauseMenu);
		}

		if (IA_Crouch) {
			EnhancedInput->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AFPSCharacter::ToggleCrouch);
		}
	}
}

void AFPSCharacter::StartJump() {
	bPressedJump = true;
}

void AFPSCharacter::StopJump() {
	bPressedJump = false;
}

void AFPSCharacter::ApplyDamage(float Amount) {
	CurrentHealth -= Amount;
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.f, MaxHealth);
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("Player Health: %.1f"), CurrentHealth));
	if (CurrentHealth <= 0.f) {
		{
			bIsDead = true;
			if (!bIsTestMode) {
				UGameplayStatics::OpenLevel(this, FName("MainMenu"));
			}
		}
	}
}

void AFPSCharacter::Heal(float Amount) {
	CurrentHealth += Amount;
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.f, MaxHealth);

	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Player Health: %.1f"), CurrentHealth));
}

void AFPSCharacter::Reload() {
	if (EquippedWeapon) {
		EquippedWeapon->Reload();
	}
}

void AFPSCharacter::HandleWeaponFired(AFPSWeaponBase* Weapon) {
	if (ProjectileClass && Weapon) {
		FVector CameraLocation;
		FRotator CameraRotation;
		GetActorEyesViewPoint(CameraLocation, CameraRotation);

		FVector TraceStart = CameraLocation;
		FVector TraceEnd = TraceStart + (CameraRotation.Vector() * 10000.0f);

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		FVector TargetLocation = TraceEnd;
		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)) {
			TargetLocation = Hit.ImpactPoint;
		}

		FVector MuzzleLocation = Weapon->GetMuzzleWorldLocation();
		FVector ShootDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();

		UWorld* World = GetWorld();
		if (World) {
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetInstigator();

			AFPSProjectile* Projectile = World->SpawnActor<AFPSProjectile>(ProjectileClass, MuzzleLocation, ShootDirection.Rotation(), SpawnParams);
			if (Projectile) {
				Projectile->Damage = Weapon->Damage;
				Projectile->FireInDirection(ShootDirection);
			}
		}
	}
}

void AFPSCharacter::Fire() {
	if (EquippedWeapon) {
		EquippedWeapon->Fire();
	}
}

void AFPSCharacter::MoveForward(const FInputActionValue& Value) {
	float MovementValue = Value.Get<float>();

	if (Controller && MovementValue != 0.0f) {
		FRotator ControlRotation = Controller->GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 0);

		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementValue);
	}
}

void AFPSCharacter::MoveRight(const FInputActionValue& Value) {
	float MovementValue = Value.Get<float>();

	if (Controller && MovementValue != 0.0f) {
		FRotator ControlRotation = Controller->GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 0);

		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementValue);
	}
}

void AFPSCharacter::Look(const FInputActionValue& Value) {
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AController* Ctrl = GetController();
	if (Cast<APlayerController>(Ctrl))
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(-LookAxisVector.Y);
	}
	else if (Ctrl)
	{
		FRotator NewControlRotation = Ctrl->GetControlRotation();
		NewControlRotation.Yaw += LookAxisVector.X;
		NewControlRotation.Pitch += -LookAxisVector.Y;
		Ctrl->SetControlRotation(NewControlRotation);
	}

}

void AFPSCharacter::Sprint(const FInputActionValue& Value) {
	bool bPressed = Value.Get<bool>();

	if (bPressed) {
		bIsSprinting = true;
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
	else {
		bIsSprinting = false;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
}

void AFPSCharacter::SwapWeapon(const FInputActionValue& Value) {
	float AxisValue = Value.Get<float>();
	GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Tries swapping"));

	if (AxisValue > 0.1f) {
		NextWeapon();
	}
	else if (AxisValue < -0.1f) {
		PrevWeapon();
	}
}

void AFPSCharacter::NextWeapon() {
	if (PrimaryWeapon && SecondaryWeapon) {
		if (EquippedWeapon) {
			EquippedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			EquippedWeapon->SetActorHiddenInGame(true);
		}

		EquippedWeapon = (EquippedWeapon == PrimaryWeapon) ? SecondaryWeapon : PrimaryWeapon;
		EquippedWeapon->SetActorHiddenInGame(false);
		EquippedWeapon->AttachToComponent(FPSCameraComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EquippedWeapon->SetActorRelativeLocation(FVector(50.0f, 20.0f, -20.0f));
		EquippedWeapon->SetActorRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	}
}

void AFPSCharacter::PrevWeapon() { NextWeapon(); }

void AFPSCharacter::StartFire() {
	bIsFiring = true;
	HandleFire();
}

void AFPSCharacter::StopFire() {
	bIsFiring = false;
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void AFPSCharacter::HandleFire() {
	if (EquippedWeapon) {
		EquippedWeapon->Fire();

		if (EquippedWeapon->FireMode == EWeaponFireMode::FullAuto && bIsFiring) {
			float TimeBetweenShots = 1.0f / EquippedWeapon->FireRate;
			GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &AFPSCharacter::HandleFire, TimeBetweenShots, false);
		}
	}
}

void AFPSCharacter::CollectItem(ECollectibleType Type) {
	if (Type == ECollectibleType::BlueSapphire) BlueSapphireCount++;
	else if (Type == ECollectibleType::RedRuby) RedRubyCount++;
	else if (Type == ECollectibleType::FinishToken) UGameplayStatics::OpenLevel(this, FName("FPSMap"));

	// Achievement
	if (!bUnlockedAllCollectibles && RedRubyCount >= 5 && BlueSapphireCount >= 5) {
		bUnlockedAllCollectibles = true;
		if (AFPSHUD* MyHUD = Cast<AFPSHUD>(GetWorld()->GetFirstPlayerController()->GetHUD()))
			MyHUD->ShowAchievement("All Collectibles Found!");
	}
}

void AFPSCharacter::OnEnemyKilled() {
	KillCount++;

	if (!bUnlocked10Kills && KillCount >= 10) {
		bUnlocked10Kills = true;
		if (AFPSHUD* MyHUD = Cast<AFPSHUD>(GetWorld()->GetFirstPlayerController()->GetHUD()))
			MyHUD->ShowAchievement("10 Kills!");
	}
	if (!bUnlocked20Kills && KillCount >= 20) {
		bUnlocked20Kills = true;
		if (AFPSHUD* MyHUD = Cast<AFPSHUD>(GetWorld()->GetFirstPlayerController()->GetHUD()))
			MyHUD->ShowAchievement("20 Kills!");
	}
	if (!bUnlocked50Kills && KillCount >= 50) {
		bUnlocked50Kills = true;
		if (AFPSHUD* MyHUD = Cast<AFPSHUD>(GetWorld()->GetFirstPlayerController()->GetHUD()))
			MyHUD->ShowAchievement("50 Kills!");
	}
}

void AFPSCharacter::HandlePauseMenu() {
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	bool bIsPaused = UGameplayStatics::IsGamePaused(GetWorld());

	if (!bIsPaused) {
		UGameplayStatics::SetGamePaused(GetWorld(), true);

		if (PauseMenuClass) {
			PauseMenuWidget = CreateWidget<UUserWidget>(PC, PauseMenuClass);
			if (PauseMenuWidget) {
				PauseMenuWidget->AddToViewport(100);
				PC->bShowMouseCursor = true;

				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(InputMode);
			}
		}
	}
	else {
		UGameplayStatics::SetGamePaused(GetWorld(), false);
		if (PauseMenuWidget) {
			PauseMenuWidget->RemoveFromParent();
			PauseMenuWidget = nullptr;
		}
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

void AFPSCharacter::ToggleCrouch() {
	if (bIsCrouchedCustom) {
		if (CanStandUp()) {
			bIsCrouchedCustom = false;
			GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
			GetCapsuleComponent()->SetCapsuleHalfHeight(DefaultCapsuleHalfHeight, true);

			TargetCameraOffset = DefaultCameraOffset;
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("STANDING UP"));
		}
	}
	else {
		bIsCrouchedCustom = true;
		GetCharacterMovement()->MaxWalkSpeed = CrouchSpeed;
		GetCapsuleComponent()->SetCapsuleHalfHeight(CrouchedHeight, true);

		TargetCameraOffset = CrouchedCameraOffset;
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("CROUCHING"));
	}
}

bool AFPSCharacter::CanStandUp() const {
	const float HeightDifference = DefaultCapsuleHalfHeight - CrouchedHeight + 5.0f;
	const FVector CurrentLocation = GetCapsuleComponent()->GetComponentLocation();
	const FVector StandingLocation = CurrentLocation + FVector(0.0f, 0.0f, HeightDifference);

	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	return !GetWorld()->OverlapBlockingTestByChannel(
		StandingLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(Radius, DefaultCapsuleHalfHeight),
		QueryParams
	);
}