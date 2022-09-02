// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/Block.h"


/**
 * 
 */
class  TChunkData
{
	// TMap<FIntVector, FBlock> Blocks;

	FIntVector ChunkSize;
	TArray<FBlock> Blocks;

private:
	int32 GetBlockIndex(const FIntVector& Position) const;
	
public:
	TChunkData(){}
	explicit TChunkData(const FIntVector& InChunkSize): ChunkSize(InChunkSize)
	{
		Blocks.Init(Air, GetBlockIndex(InChunkSize-FIntVector(1))+1);
	}

public:
	FBlock GetBlock(const FIntVector& Position) const;
	bool SetBlock(const FIntVector& Position, const FBlock& Block);
	bool RemoveBlock(const FIntVector& Position);
	TMap<FIntVector, FBlock> GetBlocks() const;

	bool IsEmpty() const;

	
};
