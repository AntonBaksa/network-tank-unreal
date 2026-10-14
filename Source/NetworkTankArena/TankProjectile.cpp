#include "TankProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TankCombat.h"
#include "TankGameplayMath.h"
#include "TankPawn.h"
#include "UObject/ConstructorHelpers.h"

ATankProjectile::ATankProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicatingMovement(false);
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(NetworkTankArena::LocalCombat::ProjectileRadius);
	// The explicit sweep handles collisions; projectiles must not obstruct tank movement.
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sphere->SetGenerateOverlapEvents(false);
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Mesh->SetStaticMesh(Ball.Object);
	Mesh->SetRelativeScale3D(FVector(0.2)); // Engine sphere diameter 100 cm -> 20 cm.
}

void ATankProjectile::Initialize(ATankPawn* Shooter, const FVector& Direction)
{
	FiringTank = Shooter;
	SetOwner(Shooter);
	Forward = Direction.GetSafeNormal();
	RemainingLifetime = NetworkTankArena::LocalCombat::ProjectileLifetime;
}

bool ATankProjectile::SweepPath(UWorld* World, const FVector& Start, const FVector& End,
	const AActor* Shooter, const AActor* Projectile, FHitResult& Hit)
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TankProjectile), false);
	if (Shooter) { Query.AddIgnoredActor(Shooter); }
	if (Projectile) { Query.AddIgnoredActor(Projectile); }
	return World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(NetworkTankArena::LocalCombat::ProjectileRadius), Query);
}

void ATankProjectile::ApplyLocalHit(const FHitResult& Hit, const FVector& Direction,
	ATankPawn* Shooter, AActor* DamageCauser)
{
	if (IsValid(DamageCauser) && DamageCauser->GetNetMode() == NM_Standalone
		&& IsValid(Hit.GetActor()) && Hit.GetActor() != Shooter)
	{
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), NetworkTankArena::LocalCombat::Damage,
			Direction, Hit, Shooter ? Shooter->GetController() : nullptr, DamageCauser, nullptr);
	}
}

void ATankProjectile::ResolveHit(const FHitResult& Hit)
{
	if (bTerminal) { return; }
	bTerminal = true; // Set before damage callbacks, which may re-enter or destroy actors.
	ApplyLocalHit(Hit, Forward, FiringTank.Get(), this);
	Destroy();
}

void ATankProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTerminal) { return; }
	if (GetNetMode() != NM_Standalone || RemainingLifetime <= 0.f)
	{
		bTerminal = true;
		Destroy();
		return;
	}
	if (!NetworkTankArena::LocalTank::IsValidDeltaTime(DeltaSeconds)) { return; }
	const float Step = FMath::Min(DeltaSeconds, RemainingLifetime);
	const FVector Start = GetActorLocation();
	const FVector End = Start + Forward * NetworkTankArena::LocalCombat::ProjectileSpeed * Step;
	FHitResult Hit;
	if (SweepPath(GetWorld(), Start, End, FiringTank.Get(), this, Hit))
	{
		SetActorLocation(Hit.Location);
		ResolveHit(Hit);
		return;
	}
	SetActorLocation(End);
	RemainingLifetime -= Step;
	if (RemainingLifetime <= 0.f)
	{
		bTerminal = true;
		Destroy();
	}
}
