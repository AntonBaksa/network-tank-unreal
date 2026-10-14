#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankDamageTestTarget.generated.h"

// Persistent local measuring target: no health, death or replicated damage.
UCLASS()
class NETWORKTANKARENA_API ATankDamageTestTarget : public AActor
{
	GENERATED_BODY()
public:
	ATankDamageTestTarget();
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Local Damage")
	int32 HitCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Local Damage")
	float TotalDamage = 0.f;
};
