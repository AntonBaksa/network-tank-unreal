#include "TankDamageTestTarget.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATankDamageTestTarget::ATankDamageTestTarget()
{
	bReplicates = false;
	SetReplicatingMovement(false);
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Box);
	Box->InitBoxExtent(FVector(50.0));
	Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetGenerateOverlapEvents(false);
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
}

float ATankDamageTestTarget::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (GetNetMode() != NM_Standalone || !CanBeDamaged() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
	{
		return 0.f;
	}
	++HitCount;
	TotalDamage += DamageAmount;
	UE_LOG(LogTemp, Display, TEXT("Local damage target %s: hits=%d total=%.0f"), *GetName(), HitCount, TotalDamage);
	return DamageAmount;
}
