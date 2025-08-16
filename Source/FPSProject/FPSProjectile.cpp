// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSProjectile.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "FPSEnemyBase.h"

// Sets default values
AFPSProjectile::AFPSProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	if (!RootComponent) {
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileSceneComponent"));
	}

	if (!CollisionComponent) {
		CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
		CollisionComponent->BodyInstance.SetCollisionProfileName(TEXT("Projectile"));
		CollisionComponent->OnComponentHit.AddDynamic(this, &AFPSProjectile::OnHit);
		CollisionComponent->InitSphereRadius(15.0f);
		RootComponent = CollisionComponent;
	}

	if (!ProjectileMovementComponent) {
		ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
		ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		ProjectileMovementComponent->InitialSpeed = 30000.0f;
		ProjectileMovementComponent->MaxSpeed = 30000.0f;
		ProjectileMovementComponent->bRotationFollowsVelocity = true;
		ProjectileMovementComponent->bShouldBounce = true;
		ProjectileMovementComponent->Bounciness = 0.3f;
		ProjectileMovementComponent->ProjectileGravityScale = 0.0f;
	}

	if (!ProjectileMeshComponent)
	{
		ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
		static const TCHAR* MeshPath = TEXT("/Game/Static/Bullet.Bullet");
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
		{
			ProjectileMeshComponent->SetStaticMesh(Mesh);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AFPSProjectile: Missing mesh at %s"), MeshPath);
		}

		static const TCHAR* MatPath = TEXT("/Game/Static/SphereMaterial.SphereMaterial");
		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, MatPath))
		{
			ProjectileMaterialInstance = UMaterialInstanceDynamic::Create(BaseMat, ProjectileMeshComponent);
			ProjectileMeshComponent->SetMaterial(0, ProjectileMaterialInstance);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AFPSProjectile: Missing material at %s"), MatPath);
		}

		ProjectileMeshComponent->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
		ProjectileMeshComponent->SetupAttachment(RootComponent);
	}

	InitialLifeSpan = 3.0f;

}

// Called when the game starts or when spawned
void AFPSProjectile::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AFPSProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AFPSProjectile::FireInDirection(const FVector& ShootDirection) {
	ProjectileMovementComponent->Velocity = ShootDirection * ProjectileMovementComponent->InitialSpeed;
}

void AFPSProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit) {
	if (OtherActor != this && OtherActor->IsA(AFPSEnemyBase::StaticClass())) {
		AFPSEnemyBase* Enemy = Cast<AFPSEnemyBase>(OtherActor);
		if (Enemy) {
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, FString::Printf(TEXT("ENEMY HIT")));
			Enemy->ReceiveDamage(Damage);

			FVector KnockbackDirection = Enemy->GetActorLocation() - GetActorLocation();
			KnockbackDirection.Normalize();
			float KnockbackStrength = 800.f;	//Just set it like that?
			Enemy->ApplyKnockback(KnockbackDirection, KnockbackStrength);
		}
	}
	Destroy();
}
