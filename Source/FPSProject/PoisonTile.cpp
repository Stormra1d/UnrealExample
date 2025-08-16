// Fill out your copyright notice in the Description page of Project Settings.


#include "PoisonTile.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h" 
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "FPSCharacter.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"

// Sets default values
APoisonTile::APoisonTile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	RootComponent = BoxComponent;
	BoxComponent->SetBoxExtent(FVector(200.0f, 200.0f, 50.0f));
	BoxComponent->SetCollisionProfileName(TEXT("Trigger"));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		MeshComponent->SetStaticMesh(Plane);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing plane mesh /Engine/BasicShapes/Plane.Plane"));
	}

	MeshComponent->SetRelativeScale3D(FVector(4.0f, 4.0f, 1.0f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.04, -50.0f));

	if (UMaterialInterface* Green = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Static/M_Green.M_Green")))
	{
		MeshComponent->SetMaterial(0, Green);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing material /Game/Static/M_Green.M_Green"));
	}

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);

	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &APoisonTile::OnBeginOverlap);

#if WITH_EDITORONLY_DATA
	BoxComponent->ShapeColor = BoxColor;
#endif
}

// Called when the game starts or when spawned
void APoisonTile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APoisonTile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TArray<AFPSCharacter*> ToRemove;
	for (auto& Elem : PoisonTimer) {
		AFPSCharacter* Player = Elem.Key;
		float& TimeLeft = Elem.Value;
		if (IsValid(Player) && TimeLeft > 0.0f) {
			Player->ApplyDamage(DamagePerSecond * DeltaTime);
			TimeLeft -= DeltaTime;
			if (TimeLeft <= 0.0f)
				ToRemove.Add(Player);
		}
		else {
			ToRemove.Add(Player);
		}
	}
	for (AFPSCharacter* Player : ToRemove)
		PoisonTimer.Remove(Player);
}

void APoisonTile::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	AFPSCharacter* Player = Cast<AFPSCharacter>(OtherActor);
	if (Player)
		PoisonTimer.Add(Player, MaxDuration);
}
