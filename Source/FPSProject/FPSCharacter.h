#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "FPSProjectile.h"
#include "FPSWeaponBase.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "CollectiblePickup.h"

class AFPSWeaponBase;

#include "FPSCharacter.generated.h"

UCLASS()
class FPSPROJECT_API AFPSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AFPSCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<class AFPSProjectile> ProjectileClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	AFPSWeaponBase* PrimaryWeapon = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	AFPSWeaponBase* SecondaryWeapon = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	AFPSWeaponBase* EquippedWeapon = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AFPSWeaponBase> WeaponClassToSpawn;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable)
	void StartJump();

	UFUNCTION(BlueprintCallable)
	void StopJump();

	UFUNCTION(BlueprintCallable)
	void Fire();

	UFUNCTION(BlueprintCallable)
	void Reload();

	UFUNCTION()
	void HandleWeaponFired(AFPSWeaponBase* Weapon);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION()
	void MoveForward(const FInputActionValue& Value);

	UFUNCTION()
	void MoveRight(const FInputActionValue& Value);

	UFUNCTION()
	void Look(const FInputActionValue& Value);

	UFUNCTION()
	void Sprint(const FInputActionValue& Value);

	UFUNCTION()
	void ToggleCrouch();

	UFUNCTION()
	void SwapWeapon(const FInputActionValue& Value);
	void NextWeapon();
	void PrevWeapon();

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* FPSCameraComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* FPSMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	FVector MuzzleOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* IA_MoveForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* IA_MoveRight;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Look;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Jump;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Fire;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Reload;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Sprint;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Swap;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Pause;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Crouch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category = "Movement")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category = "Movement")
	float SprintSpeed = 1200.0;
	bool bIsSprinting = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CrouchSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CrouchedHeight = 44.0f;

	bool bIsCrouchedCustom = false;
	FVector TargetCameraOffset;
	float DefaultCapsuleHalfHeight = 88.0f;
	FVector DefaultCameraOffset = FVector(0.0f, 0.0f, 50.0f + BaseEyeHeight);
	FVector CrouchedCameraOffset = FVector(0.0f, 0.0f, 25.0f + BaseEyeHeight);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 RedRubyCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 BlueSapphireCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	UFUNCTION()
	void CollectItem(ECollectibleType Type);

	FTimerHandle FireTimerHandle;
	bool bIsFiring = false;

	void StartFire();
	void StopFire();
	void HandleFire();

	AFPSWeaponBase* GetPrimaryWeapon() const { return PrimaryWeapon; }
	AFPSWeaponBase* GetSecondaryWeapon() const { return SecondaryWeapon; }
	AFPSWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	void SetPrimaryWeapon(AFPSWeaponBase* NewWeapon) { PrimaryWeapon = NewWeapon; }
	void SetSecondaryWeapon(AFPSWeaponBase* NewWeapon) { SecondaryWeapon = NewWeapon; }
	void SetEquippedWeapon(AFPSWeaponBase* NewWeapon) { EquippedWeapon = NewWeapon; }

	int32 KillCount = 0;
	bool bUnlocked10Kills = false;
	bool bUnlocked20Kills = false;
	bool bUnlocked50Kills = false;
	bool bUnlockedAllCollectibles = false;

	void OnEnemyKilled();

	UFUNCTION(BlueprintCallable)
	void HandlePauseMenu();

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PauseMenuClass;

	UUserWidget* PauseMenuWidget = nullptr;

	bool bIsTestMode = false;
	bool bIsDead = false;

	bool CanStandUp() const;

	bool thisEngineIsDumb = false;
};
