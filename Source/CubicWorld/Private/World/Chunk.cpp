// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Chunk.h"

bool UChunk::AddTile(const FIntVector Position, const FTile Tile)
{
	Tiles.Add(Position, Tile);
	return true;
}

bool UChunk::RemoveTile(const FIntVector Position)
{
	return static_cast<bool>(Tiles.Remove(Position));
}

const TMap<FIntVector, FTile>& UChunk::GetTiles() const
{
	return Tiles;
}

void UChunk::SetTiles(const TMap<FIntVector, FTile> InTiles)
{
	Tiles = InTiles;
}
