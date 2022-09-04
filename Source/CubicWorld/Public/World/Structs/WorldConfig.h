// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockType.h"
#include "WorldConfig.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FWorldConfig
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Chunk")
	FIntVector ChunkSize = FIntVector(16,16,16);
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Chunk")
	int32 MaxChunksZ = 4;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Chunk")
	uint8 MaxChunkRenderDistance = 16;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Tiles")
	FVector BlockSize = FVector(100.0f);
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Tiles")
	TArray<FBlockType> BlockTypes;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Material")
	UMaterialInterface* Material;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Material")
	TArray<float> LODs = {1};
	
	uint16 GetWorldBlockHeight() const
	{
		return ChunkSize.Z * MaxChunksZ;
	}
	
	FVector GetChunkWorldSize() const
	{
		return FVector(	ChunkSize.X * BlockSize.X,
										ChunkSize.Y * BlockSize.Y,
										ChunkSize.Z * BlockSize.Z);
	}

	uint32 GetMaxBlocks() const
	{
		return BlockSize.X * BlockSize.Y * BlockSize.Z;
	}
};
