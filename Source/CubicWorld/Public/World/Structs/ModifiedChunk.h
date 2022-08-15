// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tile.h"
#include "ModifiedChunk.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FModifiedChunk
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FIntVector, FTile> ModifiedTiles;

	FModifiedChunk(){};
	explicit FModifiedChunk(const TMap<FIntVector, FTile> InModifiedTiles) : ModifiedTiles(InModifiedTiles){};

	friend FArchive& operator<<( FArchive& Ar, FModifiedChunk& InModifiedChunk )
	{
		for (auto tile : InModifiedChunk.ModifiedTiles)
		{
			Ar << tile.Key << tile.Value;
		}
		return Ar;
	}
};
