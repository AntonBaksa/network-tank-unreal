#include "TankPawn.h"

#include "HealthBarUW.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "TankGameplayMath.h"
#include "TankMovementComponent.h"
#include "TankWeaponComponent.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerState.h"


ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.f);
	SetMinNetUpdateFrequency(20.f);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CollisionRoot = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->InitCapsuleSize(90.f, 100.f);
	CollisionRoot->SetCollisionProfileName(TEXT("Pawn"));
	CollisionRoot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionRoot->SetSimulatePhysics(false);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CollisionRoot);
	VisualRoot->SetRelativeLocation(FVector(0.0, 0.0, -65.0));
	ChassisVisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ChassisVisualPivot"));
	ChassisVisualPivot->SetupAttachment(VisualRoot);
	ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChassisMesh"));
	ChassisMesh->SetupAttachment(ChassisVisualPivot);
	TurretYawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretYawPivot"));
	TurretYawPivot->SetupAttachment(VisualRoot);
	TurretYawPivot->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TurretYawPivot);
	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(TurretYawPivot);
	BarrelMesh->SetRelativeLocation(FVector(70.0, 0.0, 0.0));
	for (UStaticMeshComponent* Mesh : {ChassisMesh.Get(), TurretMesh.Get(), BarrelMesh.Get()})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCanEverAffectNavigation(false);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CollisionRoot);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-60.0, 45.0, 0.0));
	CameraBoom->TargetArmLength = 1500.f;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->AddTickPrerequisiteActor(this);
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	FollowCamera->OrthoWidth = 2400.f;
	FollowCamera->bUsePawnControlRotation = false;

	TankMovement = CreateDefaultSubobject<UTankMovementComponent>(TEXT("TankMovement"));
	TankMovement->SetUpdatedComponent(CollisionRoot);
	TankWeapon = CreateDefaultSubobject<UTankWeaponComponent>(TEXT("TankWeapon"));
}

UPawnMovementComponent* ATankPawn::GetMovementComponent() const
{
	return TankMovement;
}

void ATankPawn::BeginPlay()
{
	Super::BeginPlay();
	using namespace NetworkTankArena::LocalTank;
	if (HasAuthority())
	{
		CollisionRoot->SetWorldScale3D(FVector::OneVector);
		CollisionRoot->SetWorldRotation(FRotator(0.0, NormalizeYaw(GetActorRotation().Yaw), 0.0));
		CanonicalTurretWorldYaw = ResolveTurretWorldYaw(GetActorRotation().Yaw, GetActorLocation(), FTankControlIntent{});
	}
	CollisionRoot->SetSimulatePhysics(false);
	CollisionRoot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	for (UStaticMeshComponent* Mesh : {ChassisMesh.Get(), TurretMesh.Get(), BarrelMesh.Get()})
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// Import-axis corrections belong on meshes, not the shared yaw/bob pivots.
	VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
	TurretYawPivot->SetRelativeRotation(FRotator::ZeroRotator);
	BaseVisualTransform = VisualRoot->GetRelativeTransform();
	BaseChassisTransform = ChassisVisualPivot->GetRelativeTransform();
	TankMovement->SetUpdatedComponent(CollisionRoot);
	TankMovement->InitializeGameplayPlane();
	PreviousPresentationYaw = NormalizeYaw(GetActorRotation().Yaw);
	UpdateControllerTickPrerequisite();
	ApplyPresentation(0.f);
	
	for (UStaticMeshComponent* Mesh : {ChassisMesh.Get(), TurretMesh.Get(), BarrelMesh.Get()})
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			OriginalMaterials.Add(Mesh->GetMaterial(i));
		}
	}
	
	if (UWidgetComponent* WidgetComp = FindComponentByClass<UWidgetComponent>())
	{
		WidgetComp->InitWidget();
		if (UHealthBarUW* Bar = Cast<UHealthBarUW>(WidgetComp->GetUserWidgetObject()))
		{
			Bar->SetTank(this);
		}
	}
}

void ATankPawn::SubmitControlIntent(const FTankControlIntent& Intent)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}
	CurrentIntent = NetworkTankArena::LocalTank::SanitizeIntent(Intent);
	ControlIntentAgeSeconds = 0.f;
}

void ATankPawn::ClearControlIntent()
{
	CurrentIntent = FTankControlIntent{};
	ControlIntentAgeSeconds = 0.f;
	TankMovement->Velocity = FVector::ZeroVector;
}

double ATankPawn::GetGameplayPlaneZ() const
{
	return TankMovement->GetGameplayPlaneZ();
}

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (HasAuthority())
	{
		if (!bIsDead)
		{
			if (NetworkTankArena::LocalTank::IsValidDeltaTime(DeltaTime))
			{
				ControlIntentAgeSeconds += DeltaTime;
				if (ControlIntentAgeSeconds >= ControlIntentTimeoutSeconds)
				{
					ClearControlIntent();
				}
			}
			TankMovement->SimulateMovement(CurrentIntent, DeltaTime);
			if (NetworkTankArena::LocalTank::IsValidDeltaTime(DeltaTime))
			{
				CanonicalTurretWorldYaw = NetworkTankArena::LocalTank::ResolveTurretWorldYaw(
					CanonicalTurretWorldYaw, GetActorLocation(), CurrentIntent);
			}
		}
	}
	ApplyPresentation(DeltaTime);
}

void ATankPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ATankPawn, CanonicalTurretWorldYaw);
	DOREPLIFETIME(ATankPawn, Health);
}

void ATankPawn::OnRep_CanonicalTurretWorldYaw()
{
	// Initial replication can notify before BeginPlay captures Blueprint visual offsets.
	if (HasActorBegunPlay())
	{
		ApplyPresentation(0.f);
	}
}

void ATankPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// APawn normally promotes a remote player's pawn to an autonomous proxy.
	// FRepMovement skips autonomous proxies. With no prediction, ALL client tanks
	// must be simulated proxies, including the owned tank. Ownership still belongs
	// to the controller; its Server RPC carries intent for its possessed pawn.
	SetAutonomousProxy(false);
	ClearControlIntent();
}

void ATankPawn::Restart()
{
	Super::Restart();
	ClearControlIntent();
	UpdateControllerTickPrerequisite();
}

void ATankPawn::ApplyPresentation(float DeltaTime)
{
	using namespace NetworkTankArena::LocalTank;
	if (IsValidDeltaTime(DeltaTime))
	{
		HoverPhase = FMath::Fmod(HoverPhase + 2.0 * UE_PI * NonNegativeFinite(HoverFrequencyHz) * DeltaTime, 2.0 * UE_PI);
		// Derive local banking from observed chassis motion on every machine. No intent
		// or cosmetic transforms need replication; stepped client motion is intentional.
		const float TurnRate = NonNegativeFinite(TankMovement->GetTurnRateDegreesPerSecond());
		const float ObservedTurn = TurnRate > UE_SMALL_NUMBER
			? SanitizeAxis(FMath::FindDeltaAngleDegrees(PreviousPresentationYaw, GetActorRotation().Yaw) / (DeltaTime * TurnRate)) : 0.f;
		const float TargetBank = -ObservedTurn * FMath::Min(NonNegativeFinite(MaxBankDegrees), 15.f);
		CurrentBankDegrees = FMath::FInterpTo(CurrentBankDegrees, TargetBank, DeltaTime, NonNegativeFinite(BankInterpSpeed));
		PreviousPresentationYaw = NormalizeYaw(GetActorRotation().Yaw);
	}
	const double Bob = FMath::Sin(HoverPhase) * NonNegativeFinite(HoverAmplitude);
	VisualRoot->SetRelativeLocation(BaseVisualTransform.GetLocation() + FVector(0.0, 0.0, Bob));
	ChassisVisualPivot->SetRelativeRotation(BaseChassisTransform.Rotator() + FRotator(0.0, 0.0, CurrentBankDegrees));
	TurretYawPivot->SetRelativeRotation(FRotator(0.0,
		CalculateRelativeTurretYaw(GetActorRotation().Yaw, CanonicalTurretWorldYaw), 0.0));
}

void ATankPawn::UpdateControllerTickPrerequisite()
{
	if (AController* Previous = TickController.Get())
	{
		RemoveTickPrerequisiteActor(Previous);
	}
	TickController = GetController();
	if (AController* Current = TickController.Get())
	{
		// Only controller -> pawn -> camera. Movement has no independent tick.
		AddTickPrerequisiteActor(Current);
	}
}

void ATankPawn::NotifyControllerChanged()
{
	ClearControlIntent();
	UpdateControllerTickPrerequisite();
	Super::NotifyControllerChanged();
}

void ATankPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearControlIntent();
	if (AController* Previous = TickController.Get())
	{
		RemoveTickPrerequisiteActor(Previous);
	}
	TickController.Reset();
	GetWorldTimerManager().ClearTimer(DamageFlashTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	Super::EndPlay(EndPlayReason);
}

float ATankPawn::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || DamageAmount <= 0.f)
	{
		return 0.f;
	}
	
	const int32 OldHealth = Health;
	Health -= DamageAmount;
	OnRep_Health(OldHealth);
	
	if (Health <= 0)
	{
		Die();
	}
	
	UE_LOG(LogTemp, Warning, TEXT("time=%.3f Player:%s Damage %.0f (Health=%d, bIsDead=%d)"),
		FPlatformTime::Seconds(), *GetName(), DamageAmount, Health, bIsDead);
	
	return DamageAmount;
}

void ATankPawn::OnRep_Health(int32 OldHealth)
{
	OnHealthChanged.Broadcast(Health, MaxHealth);
	
	if (Health < OldHealth)
	{
		StartDamageFlash();
	}
}

void ATankPawn::Die()
{
	bIsDead = true;
	UE_LOG(LogTemp, Warning, TEXT("time=%.3f Player%s DIE (bIsDead %d)"),
		FPlatformTime::Seconds(), *GetName(), bIsDead);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	ClearControlIntent();
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ATankPawn::Respawn, RespawnDelay, false);
}

void ATankPawn::StartDamageFlash()
{
	if (!DamageMaterial) return;
	
	for (UStaticMeshComponent* Mesh : {ChassisMesh.Get(), TurretMesh.Get(), BarrelMesh.Get()})
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, DamageMaterial);
		}
	}
	GetWorldTimerManager().SetTimer(DamageFlashTimer, this, &ATankPawn::EndDamageFlash, DamageFlashDuration, false);
}

void ATankPawn::EndDamageFlash()
{
	int32 Index = 0;
	for (UStaticMeshComponent* Mesh : {ChassisMesh.Get(), TurretMesh.Get(), BarrelMesh.Get()})
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			if (OriginalMaterials.IsValidIndex(Index))
			{
				Mesh->SetMaterial(i, OriginalMaterials[Index]);
			}
			
			Index++;
		}
	}
}

void ATankPawn::Respawn()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	Health = MaxHealth;
	OnRep_Health(0);
	bIsDead = false;
	UE_LOG(LogTemp, Warning, TEXT("time=%.3f Player%s RESPAWN (bIsDead %d)"),
		FPlatformTime::Seconds(), *GetName(), bIsDead);
}
