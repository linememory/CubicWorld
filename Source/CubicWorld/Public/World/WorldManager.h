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
#include "Structs/TileType.h"
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

	void UpdateChunk(FIntVector InChunkPosition, FIntVector InBlockPosition);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	TArray<FTileType> GetTileTypes() const;
	UFUNCTION(BlueprintCallable)
	void SetTileTypes(TArray<FTileType> InTileTypes);

	void AddActorToTrack(AActor *InActorToTrack);

	UFUNCTION(BlueprintCallable)
	FIntVector GetChunkFromTile(FIntVector TilePosition) const;
	UFUNCTION(BlueprintCallable)
	FIntVector GetTilePositionFromWorldTilePosition(const FIntVector TilePosition) const;

	UFUNCTION(BlueprintCallable)
	FIntVector GetTileCoordinatesFromWorldLocation(const FVector Location) const;

	UFUNCTION(BlueprintCallable)
	void SetTile(FIntVector InPosition, FTile InTile);

	UFUNCTION(BlueprintCallable)
	void RemoveTile(FIntVector InPosition);

	UFUNCTION(BlueprintCallable)
	bool SaveWorld();
};
