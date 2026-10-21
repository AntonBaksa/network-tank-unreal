// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthBarUW.h"
#include "TankPawn.h"
#include "Components/ProgressBar.h"


void UHealthBarUW::SetTank(ATankPawn* Tank)
{
	if (!Tank)
	{
		UE_LOG(LogTemp, Error, TEXT("Tank is NULL"));
		return;
	}
	Tank->OnHealthChanged.AddDynamic(this, &UHealthBarUW::UpdateHealthBar);
}

void UHealthBarUW::UpdateHealthBar(float Health, float MaxHealth)
{
	if (HealthBar && MaxHealth > 0.f)
    {
        HealthBar->SetPercent(FMath::Clamp(Health / MaxHealth, 0.f, 1.f));
    }
}
