// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/Block.h"


/**
 * 
 */
class  TChunkData
{
	TMap<FIntVector, FBlock> Blocks;
	
public:
	TChunkData(){}

public:
	FBlock GetBlock(const FIntVector& Position) const;
	bool SetBlock(const FIntVector& Position, const FBlock& Block);
	bool RemoveBlock(const FIntVector& Position);
	const TMap<FIntVector, FBlock>& GetBlocks() const;
	
	
};
