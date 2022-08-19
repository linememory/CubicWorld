// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "World/Structs/ChunkConfig.h"
#include "RuntimeMeshProvider.h"
#include "World/Chunk.h"
#include "World/Structs/Tile.h"
#include "RuntimeMeshProviderChunk.generated.h"

struct FTileConfig;

/**
 *
 */
UCLASS()
class CUBICWORLD_API URuntimeMeshProviderChunk final : public URuntimeMeshProvider
{
	GENERATED_BODY()

private:
	mutable FCriticalSection PropertySyncRoot;

	UPROPERTY(BlueprintGetter = GetHasCollision, BlueprintSetter = SetHasCollision)
	bool bHasCollision;

	UPROPERTY(BlueprintGetter = GetChunk, BlueprintSetter = SetChunk)
	const UChunk *Chunk;

public:
	UFUNCTION(Category = "RuntimeMesh|Providers|Box", BlueprintCallable)
	bool GetHasCollision() const;

	UFUNCTION(Category = "RuntimeMesh|Providers|Box", BlueprintCallable)
	void SetHasCollision(const bool bInHasCollision);

	UFUNCTION(Category = "RuntimeMesh|Providers|Box", BlueprintCallable)
	const UChunk *GetChunk() const;

	UFUNCTION(Category = "RuntimeMesh|Providers|Box", BlueprintCallable)
	void SetChunk(const UChunk *InChunk);

	bool bMarkedForDestroy = false;

private:
	void AddTile(FRuntimeMeshRenderableMeshData &MeshData, FTileConfig InTileConfig, FVector BlockSize, bool isFlatShaded = true, TMap<FVector, int32> *IndexLookup = nullptr);

	TArray<bool> GetSidesToRender(FIntVector InPosition, int divider = 1) const;
	TArray<FTile> GetBlocks(FIntVector InPosition, int divider = 1) const;

protected:
	virtual void Initialize() override;
	virtual FBoxSphereBounds GetBounds() override;
	virtual bool GetSectionMeshForLOD(int32 LODIndex, int32 SectionId, FRuntimeMeshRenderableMeshData &MeshData) override;
	virtual FRuntimeMeshCollisionSettings GetCollisionSettings() override;
	virtual bool HasCollisionMesh() override;
	virtual bool GetCollisionMesh(FRuntimeMeshCollisionData &CollisionData) override;
	virtual bool IsThreadSafe() override;
};

struct FTileConfig
{
	TArray<bool> SidesTORender;
	FTile Tile;
	FIntVector Position;

	FTileConfig(TArray<bool> InNeighbors, const FTile InTile, const FIntVector InPosition) : SidesTORender{InNeighbors}, Tile(InTile), Position(InPosition){};
};
