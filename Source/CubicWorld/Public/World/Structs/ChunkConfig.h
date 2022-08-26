// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldConfig.h"
#include "ChunkConfig.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FChunkConfig
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FIntVector Position = FIntVector(0);
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FWorldConfig WorldConfig = FWorldConfig();

	FChunkConfig(){}
	FChunkConfig(const FWorldConfig& InWorldConfig, const FIntVector& InPosition):
		Position(InPosition),
		WorldConfig(InWorldConfig){}
	
	FIntVector GetChunkPositionInBlocks() const
	{
		return FIntVector(Position.X * WorldConfig.ChunkSize.X,Position.Y * WorldConfig.ChunkSize.Y, Position.Z * WorldConfig.ChunkSize.Z);
	}
};