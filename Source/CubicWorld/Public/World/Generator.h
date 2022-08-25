﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/ChunkConfig.h"
#include "Structs/Tile.h"
#include "UObject/Object.h"
#include "Generator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class CUBICWORLD_API UGenerator : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void GenerateChunk(const FChunkConfig& ChunkConfig, TMap<FIntVector, FTile>& Tiles);
	
	virtual TOptional<FTile> GetTile(const FIntVector &Position, const FWorldConfig &WorldConfig)
	{
		return TOptional<FTile>();
	}
};
