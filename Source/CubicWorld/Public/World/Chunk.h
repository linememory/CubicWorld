// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/ChunkConfig.h"
#include "Structs/Tile.h"
#include "UObject/Object.h"
#include "Chunk.generated.h"

/**
 *
 */
UCLASS()
class CUBICWORLD_API UChunk final : public UObject
{
	GENERATED_BODY()

	UPROPERTY(BlueprintSetter = SetTiles)
	TMap<FIntVector, FTile> Tiles;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FChunkConfig ChunkConfig;

public:
	bool AddTile(FIntVector Position, FTile Tile);
	bool RemoveTile(FIntVector Position);
	const TMap<FIntVector, FTile> &GetTiles() const;

	UFUNCTION(BlueprintCallable)
	void SetTiles(TMap<FIntVector, FTile> InTiles);
};
