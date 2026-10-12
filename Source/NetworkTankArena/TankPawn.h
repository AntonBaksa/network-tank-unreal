#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankControlIntent.h"
#include "TankPawn.generated.h"

class UCapsuleComponent;
class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UTankMovementComponent;

UCLASS()
class NETWORKTANKARENA_API ATankPawn : public APawn
{
	GENERATED_BODY()

public:
	ATankPawn();
	virtual void Tick(float DeltaTime) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void NotifyControllerChanged() override;

	void SubmitControlIntent(const FTankControlIntent& Intent);
	void ClearControlIntent();
	double GetGameplayPlaneZ() const;
	float GetCanonicalTurretWorldYaw() const { return CanonicalTurretWorldYaw; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateControllerTickPrerequisite();
	void ApplyPresentation(float DeltaTime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CollisionRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> ChassisVisualPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ChassisMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> TurretYawPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> TurretMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BarrelMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTankMovementComponent> TankMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Tank|Presentation", meta = (ClampMin = "0", Units = "cm"))
	float HoverAmplitude = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tank|Presentation", meta = (ClampMin = "0", Units = "Hz"))
	float HoverFrequencyHz = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tank|Presentation", meta = (ClampMin = "0", ClampMax = "15", Units = "deg"))
	float MaxBankDegrees = 4.f;
	UPROPERTY(EditDefaultsOnly, Category = "Tank|Presentation", meta = (ClampMin = "0"))
	float BankInterpSpeed = 8.f;

	FTankControlIntent CurrentIntent;
	float CanonicalTurretWorldYaw = 0.f;
	FTransform BaseVisualTransform = FTransform::Identity;
	FTransform BaseChassisTransform = FTransform::Identity;
	double HoverPhase = 0.0;
	float CurrentBankDegrees = 0.f;
	TWeakObjectPtr<AController> TickController;
};
