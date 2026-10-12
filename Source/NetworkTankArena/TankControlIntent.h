#pragma once

#include "CoreMinimal.h"

// Gameplay-facing controls: no device, viewport, or controller dependencies.
struct FTankControlIntent
{
	float Throttle = 0.f;
	float Turn = 0.f;
	FVector AimWorldPoint = FVector::ZeroVector;
	bool bHasAimWorldPoint = false;
};
