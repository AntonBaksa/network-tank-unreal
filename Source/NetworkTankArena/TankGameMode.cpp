#include "TankGameMode.h"

#include "TankPawn.h"
#include "TankDamageTestTarget.h"
#include "TankPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATankGameMode::ATankGameMode()
{
	// Reuse the established Blueprint visual composition and input asset assignments.
	static ConstructorHelpers::FClassFinder<ATankPawn> TankClass(TEXT("/Game/Blueprints/BP_Tank"));
	static ConstructorHelpers::FClassFinder<ATankPlayerController> ControllerClass(TEXT("/Game/Blueprints/BP_TankPlayerController"));
	DefaultPawnClass = TankClass.Succeeded() ? TankClass.Class.Get() : ATankPawn::StaticClass();
	PlayerControllerClass = ControllerClass.Succeeded() ? ControllerClass.Class.Get() : ATankPlayerController::StaticClass();
}

void ATankGameMode::StartPlay()
{
	// A persistent local lab fixture. A placed target replaces this default fixture.
	// Networked play retains the Lab 2 movement/aim baseline with no combat actors.
	if (GetNetMode() == NM_Standalone && !TActorIterator<ATankDamageTestTarget>(GetWorld()))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		GetWorld()->SpawnActor<ATankDamageTestTarget>(LocalDamageTargetLocation, FRotator::ZeroRotator, Params);
	}
	Super::StartPlay();
}
