// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChunkData.h"
#include "Structs/ChunkConfig.h"
#include "Structs/Block.h"
#include "UObject/Object.h"
#include "Chunk.generated.h"

/**
 *
 */
UCLASS(BlueprintType, Blueprintable)
class CUBICWORLD_API UChunk final : public UObject
{
	GENERATED_BODY()
private:
	//UPROPERTY(BlueprintSetter = SetBlocks, BlueprintGetter = GetBlocks)
	TChunkData Blocks;

public:
	TMap<FIntVector, UChunk*>* WorldChunks;
	
	bool bIsReady = false;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FChunkConfig ChunkConfig;

public:
	UFUNCTION(BlueprintCallable)
	bool AddBlock(FIntVector Position, FBlock Tile);
	UFUNCTION(BlueprintCallable)
	bool RemoveBlock(FIntVector Position);

	UFUNCTION()
	FBlock GetBlock(const FIntVector& Position) const;
	
	UFUNCTION(BlueprintCallable)
	void SetBlocks(TMap<FIntVector, FBlock> InBlocks);

	UFUNCTION(BlueprintCallable)
	const TMap<FIntVector, FBlock>& GetBlocks() const;
};
