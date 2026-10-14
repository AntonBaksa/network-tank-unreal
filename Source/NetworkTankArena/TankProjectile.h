#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankProjectile.generated.h"

class ATankPawn;
class USphereComponent;

// Local straight-line projectile. Collision is an explicit radius sweep per simulation step.
UCLASS()
class NETWORKTANKARENA_API ATankProjectile : public AActor
{
	GENERATED_BODY()
public:
	ATankProjectile();
	virtual void Tick(float DeltaSeconds) override;

	// Launch clearance and flight deliberately use the same collision shape/filter.
	static bool SweepPath(UWorld* World, const FVector& Start, const FVector& End,
		const AActor* Shooter, const AActor* Projectile, FHitResult& Hit);
	static void ApplyLocalHit(const FHitResult& Hit, const FVector& Direction,
		ATankPawn* Shooter, AActor* DamageCauser);

private:
	friend class UTankWeaponComponent;
	friend class FTankLocalProjectileTest;
	friend class FTankLocalWeaponTest;
	void Initialize(ATankPawn* Shooter, const FVector& Direction);
	void ResolveHit(const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "Tank|Projectile")
	TObjectPtr<USphereComponent> Sphere;
	TWeakObjectPtr<ATankPawn> FiringTank;
	FVector Forward = FVector::ForwardVector;
	float RemainingLifetime = 0.f;
	bool bTerminal = false;
};
