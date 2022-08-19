// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshProvider.h"
#include "World/Chunk.h"
#include "World/Structs/Tile.h"
#include "RuntimeMeshProviderChunk.generated.h"

struct FBlockConfig;
struct FSides;

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
	void AddTile(FRuntimeMeshRenderableMeshData &MeshData, const FBlockConfig& InTileConfig);
	
	FSides GetSidesToRender(FIntVector InPosition, int divider = 1) const;
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


struct FSides
{
	bool Top = false;		// Z+
	bool Bottom = false;	// Z-
	bool Front = false;		// Y+
	bool Back = false;		// Y-
	bool Right = false;		// X+
	bool Left = false;		// X-

	FSides(){}
	explicit FSides(const bool Sides): Top(Sides), Bottom(Sides), Front(Sides), Back(Sides), Right(Sides), Left(Sides) {}
	FSides(const bool InTop, const bool InBottom, const bool InFront, const bool InBack, const bool InRight, const bool InLeft)
		: Top(InTop), Bottom(InBottom), Front(InFront), Back(InBack), Right(InRight), Left(InLeft) {};
};

struct FBlockConfig
{
	FIntVector Position;
	FSides SidesTORender;
	FVector Size;
	FTile Tile;

	FBlockConfig(FSides& InNeighbors, const FTile& InTile, const FIntVector& InPosition, const FVector& InSize) : Position(InPosition), SidesTORender{InNeighbors}, Size(InSize), Tile(InTile) {};
};
