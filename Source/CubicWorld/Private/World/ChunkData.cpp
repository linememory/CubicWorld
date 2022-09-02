// Fill out your copyright notice in the Description page of Project Settings.


#include "World/ChunkData.h"


int32 TChunkData::GetBlockIndex(const FIntVector& Position) const
{
	if(	Position.X < 0 || Position.X > ChunkSize.X ||
		Position.Y < 0 || Position.Y > ChunkSize.Y ||
		Position.Z < 0 || Position.Z > ChunkSize.Z)
	{
		return -1;
	}
	return Position.Z * ChunkSize.X * ChunkSize.Y + Position.Y * ChunkSize.X + Position.X;
}

FBlock TChunkData::GetBlock(const FIntVector& Position) const
{
	const int32 index = GetBlockIndex(Position);
	return index >= 0 && index < Blocks.Num() ? Blocks[index] : Air;
}

bool TChunkData::SetBlock(const FIntVector& Position, const FBlock& Block)
{
	if(const int32 index = GetBlockIndex(Position); index >= 0 && index < Blocks.Num())
	{
		if(Block == Air)
		{
			return RemoveBlock(Position);
		}
		Blocks[index] = Block;
		return true;
	}
	return false;
}

void TChunkData::SetBlocks(const TArray<FBlock>& InBlocks)
{
	Blocks = InBlocks;
}

const TArray<FBlock>& TChunkData::GetBlocks()
{
	return Blocks;
}

bool TChunkData::RemoveBlock(const FIntVector& Position)
{
	if(const int32 index = GetBlockIndex(Position); index >= 0 && index < Blocks.Num())
	{
		Blocks[index] = Air;
		return true;
	}
	return false;
}

bool TChunkData::IsEmpty() const
{
	return Blocks.Num() == 0;
}

