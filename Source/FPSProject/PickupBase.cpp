// Fill out your copyright notice in the Description page of Project Settings.


#include "PickupBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FPSCHaracter.h"
#include "Engine/Engine.h"

// Sets default values
APickupBase::APickupBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(40.0f);
	CollisionComp->SetCollisionProfileName("OverlapAllDynamic");
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APickupBase::HandleOverlap);
}

// Called when the game starts or when spawned
void APickupBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APickupBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APickupBase::OnCollected(AFPSCharacter* Character) {
	GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Wrong OnCollected"));
	//Base: overwritten in children
}

void APickupBase::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	AFPSCharacter* Character = Cast<AFPSCharacter>(OtherActor);
	if (Character) {
		OnCollected(Character);
		Destroy();
	}
}

