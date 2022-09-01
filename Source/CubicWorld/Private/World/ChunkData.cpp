// Fill out your copyright notice in the Description page of Project Settings.


#include "World/ChunkData.h"


FBlock TChunkData::GetBlock(const FIntVector& Position) const
{
	const auto Block = Blocks.Find(Position);;
	return Block != nullptr ? *Block : Air;
}

bool TChunkData::SetBlock(const FIntVector& Position, const FBlock& Block)
{
	if(Block == Air)
	{
		return RemoveBlock(Position);
	}
	Blocks.Add(Position, Block);
	return true;
}

bool TChunkData::RemoveBlock(const FIntVector& Position)
{
	return Blocks.Remove(Position) > 0 ? true : false;
}

const TMap<FIntVector, FBlock>& TChunkData::GetBlocks() const
{
	return Blocks;
}

bool TChunkData::IsEmpty() const
{
	return Blocks.Num() == 0;
}

