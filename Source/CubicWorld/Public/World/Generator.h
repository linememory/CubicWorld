// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/ChunkConfig.h"
#include "Structs/Tile.h"
#include "UObject/Object.h"
#include "Generator.generated.h"

USTRUCT(BlueprintType)
struct FTileOrNull
{
	GENERATED_BODY()
	bool hasTile = false;

private:
	FTile Tile;

public:
	FTileOrNull() {}
	explicit FTileOrNull(const FTile &InTile)
	{
		SetTile(InTile);
	}
	void SetTile(const FTile &InTile)
	{
		Tile = InTile;
		hasTile = true;
	}

	FTile *GetTile()
	{
		return &Tile;
	}
};

/**
 *
 */
UCLASS(BlueprintType)
class CUBICWORLD_API UGenerator : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	TMap<FIntVector, FTile> GenerateChunk(FChunkConfig ChunkConfig, TMap<FIntVector, FTile> InTiles = {});

	UFUNCTION()
	virtual FTileOrNull GetTile(const FIntVector &Position, FWorldConfig &WorldConfig)
	{
		return FTileOrNull();
	}
};
