#include "TankWeaponComponent.h"

#include "Engine/World.h"
#include "TankGameplayMath.h"
#include "TankPawn.h"
#include "TankProjectile.h"

UTankWeaponComponent::UTankWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UTankWeaponComponent::RequestLocalFire()
{
	if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() == NM_Standalone)
	{
		bFireRequested = true;
	}
}

NetworkTankArena::LocalCombat::FFiringPose UTankWeaponComponent::GetFiringPose() const
{
	const ATankPawn* Tank = CastChecked<ATankPawn>(GetOwner());
	return NetworkTankArena::LocalCombat::CalculateFiringPose(Tank->GetActorLocation(),
		Tank->GetCanonicalTurretWorldYaw(), GameplayTurretHeight, GameplayMuzzleDistance);
}

void UTankWeaponComponent::ProcessLocalFireRequest()
{
	const bool bRequested = bFireRequested;
	bFireRequested = false; // Rejected requests are never deferred until the cooldown ends.
	ATankPawn* Tank = Cast<ATankPawn>(GetOwner());
	if (!bRequested || !IsValid(Tank) || !Tank->HasAuthority() || GetNetMode() != NM_Standalone)
	{
		return;
	}
	if (!FMath::IsFinite(GameplayTurretHeight) || !FMath::IsFinite(GameplayMuzzleDistance)
		|| GameplayMuzzleDistance < 0.f) { return; }
	const auto Pose = GetFiringPose();
	if (!NetworkTankArena::LocalTank::IsFiniteVector(Pose.Muzzle)
		|| !NetworkTankArena::LocalTank::IsFiniteVector(Pose.TurretOrigin)
		|| !NetworkTankArena::LocalTank::IsFiniteVector(Pose.Forward)
		|| !Cooldown.TryConsume(GetWorld()->GetTimeSeconds())) { return; }

	FHitResult Hit;
	if (ATankProjectile::SweepPath(GetWorld(), Pose.TurretOrigin, Pose.Muzzle, Tank, nullptr, Hit))
	{
		// Obstruction consumes the shot and receives its one damage result. Never spawn beyond cover.
		ATankProjectile::ApplyLocalHit(Hit, Pose.Forward, Tank, Tank);
		return;
	}
	const FTransform Transform(Pose.Forward.Rotation(), Pose.Muzzle);
	ATankProjectile* Projectile = GetWorld()->SpawnActorDeferred<ATankProjectile>(
		ATankProjectile::StaticClass(), Transform, Tank, Tank, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->Initialize(Tank, Pose.Forward);
		Projectile->FinishSpawning(Transform);
	}
}
