#include "TankPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TankGameplayMath.h"
#include "TankPawn.h"
#include "UnrealClient.h"

void ATankPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ApplyTankInputContext();
}

void ATankPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	ApplyTankInputContext();
}

void ATankPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}
	if (ThrottleAction)
	{
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ATankPlayerController::UpdateThrottle);
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ATankPlayerController::ClearThrottle);
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Canceled, this, &ATankPlayerController::ClearThrottle);
	}
	if (TurnAction)
	{
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ATankPlayerController::UpdateTurn);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Completed, this, &ATankPlayerController::ClearTurn);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Canceled, this, &ATankPlayerController::ClearTurn);
	}
}

void ATankPlayerController::UpdateThrottle(const FInputActionValue& Value)
{
	CachedThrottle = Value.GetValueType() == EInputActionValueType::Axis1D
		? NetworkTankArena::LocalTank::SanitizeAxis(Value.Get<float>()) : 0.f;
}

void ATankPlayerController::UpdateTurn(const FInputActionValue& Value)
{
	CachedTurn = Value.GetValueType() == EInputActionValueType::Axis1D
		? NetworkTankArena::LocalTank::SanitizeAxis(Value.Get<float>()) : 0.f;
}

void ATankPlayerController::ClearThrottle()
{
	CachedThrottle = 0.f;
}

void ATankPlayerController::ClearTurn()
{
	CachedTurn = 0.f;
}

void ATankPlayerController::ClearCachedInput()
{
	ClearThrottle();
	ClearTurn();
	if (ATankPawn* Tank = Cast<ATankPawn>(GetPawn()))
	{
		Tank->ClearControlIntent();
	}
}

void ATankPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	ClearCachedInput();
}

void ATankPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime); // Processes this frame's Enhanced Input before assembling intent.
	ATankPawn* Tank = Cast<ATankPawn>(GetPawn());
	if (!IsLocalController() || !IsValid(Tank))
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
	if (!Viewport || !Viewport->HasFocus())
	{
		ClearCachedInput();
		Tank->SubmitControlIntent(FTankControlIntent{});
		return;
	}

	FTankControlIntent Intent;
	Intent.Throttle = CachedThrottle;
	Intent.Turn = CachedTurn;
	FVector RayOrigin;
	FVector RayDirection;
	if (DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		Intent.bHasAimWorldPoint = NetworkTankArena::LocalTank::TryIntersectAimPlane(
			RayOrigin, RayDirection, Tank->GetGameplayPlaneZ(), Intent.AimWorldPoint);
	}
	Tank->SubmitControlIntent(Intent);
}

void ATankPlayerController::SetPawn(APawn* InPawn)
{
	ClearCachedInput();
	RemoveTankInputContext();
	Super::SetPawn(InPawn);
	ApplyTankInputContext();
}

void ATankPlayerController::ApplyTankInputContext()
{
	if (!IsLocalController() || !Cast<ATankPawn>(GetPawn()))
	{
		return;
	}
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem || !TankMappingContext)
	{
		return; // Asset references are intentionally supplied by a later Blueprint child.
	}
	if (MappingSubsystem.Get() == Subsystem && AppliedMappingContext == TankMappingContext)
	{
		return;
	}
	RemoveTankInputContext();
	Subsystem->AddMappingContext(TankMappingContext, 0);
	MappingSubsystem = Subsystem;
	AppliedMappingContext = TankMappingContext;
}

void ATankPlayerController::RemoveTankInputContext()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get())
	{
		if (AppliedMappingContext)
		{
			Subsystem->RemoveMappingContext(AppliedMappingContext);
		}
	}
	MappingSubsystem.Reset();
	AppliedMappingContext = nullptr;
}

void ATankPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCachedInput();
	RemoveTankInputContext();
	Super::EndPlay(EndPlayReason);
}
