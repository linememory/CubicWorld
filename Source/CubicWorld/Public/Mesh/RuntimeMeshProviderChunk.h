// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshProvider.h"
#include "World/Chunk.h"
#include "World/Structs/Block.h"
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

	UPROPERTY()
	TArray<FVector> BlockVertices;

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
	static uint32 AddVertex(FRuntimeMeshRenderableMeshData& MeshData, const FVector& InPosition, const FVector& InNormal, const FVector& InTangent, const FVector2f& InUV, const FVector2f& InTexCoord, const FColor& InColor = FColor::White);
	static void AddQuad(FRuntimeMeshRenderableMeshData &MeshData, const FVector& Vertex1, const FVector& Vertex2, const FVector& Vertex3, const FVector& Vertex4,
					const FVector& Normal, const FVector& Tangent,
					const FVector2f TextureOffset, const FVector2f UVMultiplication,
					const FColor& Color);
	void GreedyMesh(FRuntimeMeshRenderableMeshData& MeshData);
	void GreedyMesh(FRuntimeMeshRenderableMeshData& MeshData, uint32 LODIndex);
	void SimpleMesh(FRuntimeMeshRenderableMeshData& MeshData, uint32 LODIndex);
	
	FSides GetSidesToRender(FIntVector InPosition, int divider = 1) const;
	TArray<FBlock> GetBlocks(FIntVector InPosition, int divider = 1) const;

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
	uint8 Sides = 0x00000000;

	enum ESide
	{
		Top		= 1,
		Bottom	= Top << 1,
		Front	= Top << 2,
		Back	= Top << 3,
		Right	= Top << 4,
		Left	= Top << 5
	};

	FSides(){}
	explicit FSides(const bool InSides): Sides(InSides)
	{
		if(InSides)
		{
			Sides = (1 << 6) - 1;
		} else
		{
			Sides = 0;
		}
	}
	FSides(const bool InTop, const bool InBottom, const bool InFront, const bool InBack, const bool InRight, const bool InLeft)
	{
		Sides = InTop | InBottom | InFront | InBack | InRight | InLeft;
	}

	bool HasSide(const ESide InSide) const
	{
		return InSide == (InSide & Sides);
	}

	void SetSide(const ESide InSide, const bool Unset = false)
	{
		if(Unset)
		{
			Sides &= (~InSide);
		} else
		{
			Sides |= InSide;
		}
	}

	bool operator&&(const uint8 InSide) const
	{
		return Sides && InSide;
	}

	// bool operator==(const uint8 InSide) const
	// {
	// 	return Sides && InSide;
	// }
	bool operator==(const FSides& rhs) const
	{
		return Sides && rhs.Sides;
	}
};

struct FBlockConfig
{
	FIntVector Position;
	FSides SidesToRender;
	FVector Size;
	FBlock Tile;

	FBlockConfig(FSides& InNeighbors, const FBlock& InTile, const FIntVector& InPosition, const FVector& InSize) : Position(InPosition), SidesToRender{InNeighbors}, Size(InSize), Tile(InTile) {};
};
