// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarUW.generated.h"

class ATankPawn;
class UProgressBar;

/**
 * 
 */
UCLASS()
class NETWORKTANKARENA_API UHealthBarUW : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void SetTank(ATankPawn* Tank);
	
	UFUNCTION()
	void UpdateHealthBar(float Health, float MaxHealth);
	
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
};
