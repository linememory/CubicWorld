// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Trackable.h"

#include "Kismet/GameplayStatics.h"
#include "World/WorldManager.h"


UTrackable::UTrackable()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTrackable::BeginPlay()
{
	Super::BeginPlay();
	TArray<AActor*> actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorldManager::StaticClass(), actors);
	for (const auto actor : actors)
	{
		static_cast<AWorldManager*>(actor)->TrackerComponents.Add(this);
	}
}

void UTrackable::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

