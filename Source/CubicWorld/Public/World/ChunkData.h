// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/Block.h"

struct Iterator;

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
	bool IsEmpty() const;

	TArray<FBlock>::RangedForIteratorType begin() { return Blocks.begin(); }
	TArray<FBlock>::RangedForIteratorType end()   { return Blocks.end(); }

	TArray<FBlock>::RangedForConstIteratorType begin() const{ return Blocks.begin(); }
	TArray<FBlock>::RangedForConstIteratorType end() const  { return Blocks.end(); }
	
};

