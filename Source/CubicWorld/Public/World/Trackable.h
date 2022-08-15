// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Trackable.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CUBICWORLD_API UTrackable final : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTrackable();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsTrackable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 ChunkRenderDistance = 1;

	FIntVector LastWorldPosition;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction *ThisTickFunction) override;
};
