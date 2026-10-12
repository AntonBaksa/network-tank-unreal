#include "TankPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "TankGameplayMath.h"
#include "TankMovementComponent.h"

ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = false;
	SetReplicatingMovement(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
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
}

UPawnMovementComponent* ATankPawn::GetMovementComponent() const
{
	return TankMovement;
}

void ATankPawn::BeginPlay()
{
	Super::BeginPlay();
	using namespace NetworkTankArena::LocalTank;
	CollisionRoot->SetWorldScale3D(FVector::OneVector);
	CollisionRoot->SetWorldRotation(FRotator(0.0, NormalizeYaw(GetActorRotation().Yaw), 0.0));
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
	CanonicalTurretWorldYaw = ResolveTurretWorldYaw(GetActorRotation().Yaw, GetActorLocation(), FTankControlIntent{});
	UpdateControllerTickPrerequisite();
	ApplyPresentation(0.f);
}

void ATankPawn::SubmitControlIntent(const FTankControlIntent& Intent)
{
	CurrentIntent = NetworkTankArena::LocalTank::SanitizeIntent(Intent);
}

void ATankPawn::ClearControlIntent()
{
	CurrentIntent = FTankControlIntent{};
}

double ATankPawn::GetGameplayPlaneZ() const
{
	return TankMovement->GetGameplayPlaneZ();
}

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!NetworkTankArena::LocalTank::IsValidDeltaTime(DeltaTime))
	{
		TankMovement->SimulateMovement(CurrentIntent, DeltaTime);
		return;
	}
	TankMovement->SimulateMovement(CurrentIntent, DeltaTime);
	CanonicalTurretWorldYaw = NetworkTankArena::LocalTank::ResolveTurretWorldYaw(
		CanonicalTurretWorldYaw, GetActorLocation(), CurrentIntent);
	ApplyPresentation(DeltaTime);
}

void ATankPawn::ApplyPresentation(float DeltaTime)
{
	using namespace NetworkTankArena::LocalTank;
	if (IsValidDeltaTime(DeltaTime))
	{
		HoverPhase = FMath::Fmod(HoverPhase + 2.0 * UE_PI * NonNegativeFinite(HoverFrequencyHz) * DeltaTime, 2.0 * UE_PI);
		const float TargetBank = -CurrentIntent.Turn * FMath::Min(NonNegativeFinite(MaxBankDegrees), 15.f);
		CurrentBankDegrees = FMath::FInterpTo(CurrentBankDegrees, TargetBank, DeltaTime, NonNegativeFinite(BankInterpSpeed));
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
	Super::EndPlay(EndPlayReason);
}
