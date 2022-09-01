// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Chunk.h"

bool UChunk::AddBlock(const FIntVector Position, const FBlock Tile)
{
	Blocks.SetBlock(Position, Tile);
	return true;
}

bool UChunk::RemoveBlock(const FIntVector Position)
{
	return Blocks.RemoveBlock(Position);
}

FBlock UChunk::GetBlock(const FIntVector& Position) const
{
	return Blocks.GetBlock(Position);
}

const TMap<FIntVector, FBlock>& UChunk::GetBlocks() const
{
	return Blocks.GetBlocks();
}

void UChunk::SetBlocks(const TMap<FIntVector, FBlock> InBlocks)
{
	for(auto block : InBlocks)
	{
		Blocks.SetBlock(block.Key, block.Value);
	}
}

