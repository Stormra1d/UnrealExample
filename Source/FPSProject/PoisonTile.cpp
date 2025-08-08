// Fill out your copyright notice in the Description page of Project Settings.


#include "PoisonTile.h"
#include "Components/BoxComponent.h"
#include "FPSCharacter.h"
#include "Engine/Engine.h"

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh>PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded()) {
		MeshComponent->SetStaticMesh(PlaneMesh.Object);
	}

	MeshComponent->SetRelativeScale3D(FVector(4.0f, 4.0f, 1.0f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.04, -50.0f));

	static ConstructorHelpers::FObjectFinder<UMaterial>GreenMat(TEXT("/Game/Static/M_Green.M_Green"));
	if (GreenMat.Succeeded()) {
		MeshComponent->SetMaterial(0, GreenMat.Object);
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
