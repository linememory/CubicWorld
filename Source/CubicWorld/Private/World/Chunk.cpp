﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Chunk.h"

bool UChunk::AddBlock(const FIntVector Position, const FBlock Tile)
{
	Blocks.Add(Position, Tile);
	return true;
}

bool UChunk::RemoveBlock(const FIntVector Position)
{
	return static_cast<bool>(Blocks.Remove(Position));
}

const TMap<FIntVector, FBlock>& UChunk::GetBlocks() const
{
	return Blocks;
}

void UChunk::SetBlocks(const TMap<FIntVector, FBlock> InTiles)
{
	Blocks = InTiles;
}

