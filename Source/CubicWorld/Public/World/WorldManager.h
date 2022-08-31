// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Chunk.h"
#include "ChunkStorage.h"
#include "Generator.h"
#include "GeneratorRunner.h"
#include "Trackable.h"
#include "GameFramework/Actor.h"
#include "Mesh/ChunkMesh.h"
#include "Structs/ModifiedChunk.h"
#include "Structs/BlockType.h"
#include "WorldManager.generated.h"

UCLASS(BlueprintType, Blueprintable)
class CUBICWORLD_API AWorldManager final : public AActor
{
	GENERATED_BODY()
private:
	UPROPERTY()
	UGenerator *Generator;
	FGeneratorRunner *GeneratorRunner;
	UPROPERTY()
	UChunkStorage *ChunkStorage;

	UPROPERTY()
	TMap<FIntVector, UChunk *> Chunks;
	UPROPERTY()
	TArray<FIntVector> ModifiedChunks2;
	UPROPERTY()
	TMap<FIntVector, FModifiedChunk> ModifiedChunks;
	UPROPERTY()
	TMap<FIntVector, AChunkMesh *> ChunkMeshes;

	UPROPERTY()
	TArray<FIntVector> ChunksToLoad;
	UPROPERTY()
	TArray<FIntVector> ChunksToUnload;
	UPROPERTY()
	TArray<FIntVector> ChunksToRebuild;
	TQueue<UChunk *> ChunkMeshesToGenerate;

	float MillisSinceLastSave = 0;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UTrackable *> TrackerComponents;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CubicWorld")
	TSubclassOf<UGenerator> GeneratorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CubicWorld")
	FWorldConfig WorldConfig;

public:
	// Sets default values for this actor's properties
	AWorldManager();
	virtual ~AWorldManager() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;

private:
	FIntVector WorldToLocalPosition(FVector InPosition) const;
	void UpdateVisibleChunks();
	void GenerateChunks();
	void GenerateChunkMeshes();
	void UnloadChunks();

	void RemoveBlock(const FIntVector& InChunkPosition, const FIntVector& InBlockPosition);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	TArray<FBlockType> GetBlockTypes() const;
	UFUNCTION(BlueprintCallable)
	void SetBlockTypes(TArray<FBlockType> InBlockTypes);

	void AddActorToTrack(AActor *InActorToTrack);

	UFUNCTION(BlueprintCallable)
	FIntVector GetChunkPositionFromBlockWorldCoordinates(const FIntVector& BlockPosition) const;
	UFUNCTION(BlueprintCallable)
	FIntVector GetBlockPositionFromWorldBlockCoordinates(const FIntVector& BlockPosition) const;

	UFUNCTION(BlueprintCallable)
	FIntVector GetBlockCoordinatesFromWorldLocation(const FVector& Location) const;

	UFUNCTION(BlueprintCallable)
	FBlock GetBlock(const FIntVector& InPosition) const;

	UFUNCTION(BlueprintCallable)
	void SetBlock(const FIntVector& InPosition, const FBlock& InBlock);

	UFUNCTION(BlueprintCallable)
	FBlock RemoveBlock(const FIntVector& InPosition);

	UFUNCTION(BlueprintCallable)
	bool SaveWorld();

	UFUNCTION(BlueprintCallable)
	void ShowDebugLines(bool Blocks = false, bool Grid = false) const;
};
