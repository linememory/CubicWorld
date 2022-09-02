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
	const FIntVector ChunkPosition = ChunkConfig.Position;
	const FIntVector ChunkSize = ChunkConfig.WorldConfig.ChunkSize;
	if(Position.X < 0)
	{
		if(const auto chunk = WorldChunks->Find(ChunkPosition+FIntVector(-1,0,0)); chunk != nullptr && *chunk != nullptr)
		{
			return (*chunk)->GetBlock(Position+FIntVector{ChunkSize.X, 0, 0});
		}
	}
	if(Position.X >= ChunkSize.X)
	{
		if(const auto chunk = WorldChunks->Find(ChunkPosition+FIntVector(1,0,0)); chunk != nullptr && *chunk != nullptr)
		{
			return (*chunk)->GetBlock(Position-FIntVector{ChunkSize.X, 0, 0});
		}
	}
	if(Position.Y < 0)
	{
		if(const auto chunk = WorldChunks->Find(ChunkPosition+FIntVector(0,-1,0)); chunk != nullptr && *chunk != nullptr)
		{
			return (*chunk)->GetBlock(Position+FIntVector{0, ChunkSize.Y, 0});
		}
	}
	if(Position.Y >= ChunkSize.Y)
	{
		if(const auto chunk = WorldChunks->Find(ChunkPosition+FIntVector(0,1,0)); chunk != nullptr && *chunk != nullptr)
		{
			return (*chunk)->GetBlock(Position-FIntVector{0, ChunkSize.Y, 0});
		}
	}
	if(Position.Z < 0)
	{
		if(const auto chunk = WorldChunks->Find(ChunkPosition+FIntVector(0,0,-1)); chunk != nullptr && *chunk != nullptr)
		{
			return (*chunk)->GetBlock(Position+FIntVector{0, 0, ChunkSize.Z});
		}
	}
	if(Position.Z >= ChunkSize.Z)
	{
		if(const auto chunk = WorldChunks->Find(ChunkPosition+FIntVector(0,0,1)); chunk != nullptr && *chunk != nullptr)
		{
			return (*chunk)->GetBlock(Position-FIntVector{0, 0, ChunkSize.Z});
		}
	}
	return Blocks.GetBlock(Position);
}

// TMap<FIntVector, FBlock> UChunk::GetBlocks() const
// {
// 	return Blocks.GetBlocks();
// }

void UChunk::SetBlocks(const TMap<FIntVector, FBlock> InBlocks)
{
	for(auto block : InBlocks)
	{
		Blocks.SetBlock(block.Key, block.Value);
	}
	bIsReady = true;
}

