// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Block.h"
#include "ModifiedChunk.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FModifiedChunk
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FIntVector, FBlock> ModifiedBlocks;

	FModifiedChunk(){};
	explicit FModifiedChunk(const TMap<FIntVector, FBlock> InModifiedBlocks) : ModifiedBlocks(InModifiedBlocks){};

	friend FArchive& operator<<( FArchive& Ar, FModifiedChunk& InModifiedChunk )
	{
		for (auto block : InModifiedChunk.ModifiedBlocks)
		{
			Ar << block.Key << block.Value;
		}
		return Ar;
	}
};
