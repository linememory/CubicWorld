// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Chunk.h"
#include "UObject/Object.h"
#include "ChunkStorage.generated.h"

/**
 *
 */
UCLASS()
class CUBICWORLD_API UChunkStorage : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString StoragePath;

public:
	UChunkStorage();
	bool SaveChunk(const FIntVector& InPosition, const TChunkData& InBlocks) const;
	TOptional<TChunkData> LoadChunk(const FIntVector &InPosition) const;
};
