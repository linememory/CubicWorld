// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tile.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FTile
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
    uint8 TileID = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsVisible = true;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 health = UINT8_MAX;

	FTile(){};
	explicit FTile(const uint8 InTileID):TileID(InTileID){}

	friend FArchive& operator<<( FArchive& Ar, FTile& Tile )
	{
		return Ar << Tile.TileID << Tile.health << Tile.bIsVisible;
	}

	bool operator==(const FTile& rhs) const
	{
		return TileID == rhs.TileID && bIsVisible == rhs.bIsVisible;
	}

	bool operator!=(const FTile& rhs) const
	{
		return !(*this == rhs);
	}
};
