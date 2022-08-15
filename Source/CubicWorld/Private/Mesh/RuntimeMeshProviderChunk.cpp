﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Mesh/RuntimeMeshProviderChunk.h"
#include "Globals.h"

DECLARE_CYCLE_STAT(TEXT("Generate chunk mesh"), STAT_GenerateMesh, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Generate chunk collsion mesh"), STAT_GenerateCollisionMesh, STATGROUP_CubicWorld);

DECLARE_CYCLE_STAT(TEXT("Generate tile mesh"), STAT_GenerateTileMesh, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Generate tile collsion mesh"), STAT_GenerateTileCollisionMesh, STATGROUP_CubicWorld);

bool URuntimeMeshProviderChunk::GetHasCollision() const
{
	FScopeLock Lock(&PropertySyncRoot);
	return bHasCollision;
}

void URuntimeMeshProviderChunk::SetHasCollision(const bool bInHasCollision)
{
	FScopeLock Lock(&PropertySyncRoot);
	if(bHasCollision != bInHasCollision)
	{
		bHasCollision = bInHasCollision;
		MarkCollisionDirty();
	}
}

const UChunk* URuntimeMeshProviderChunk::GetChunk() const
{
	return Chunk;
}

void URuntimeMeshProviderChunk::SetChunk(const UChunk* InChunk)
{
	Chunk = InChunk;
}



void URuntimeMeshProviderChunk::Initialize()
{
	FScopeLock Lock(&PropertySyncRoot);
	const FWorldConfig& WorldConfig = Chunk->ChunkConfig.WorldConfig;
	
	FRuntimeMeshLODProperties LODProperties;
	LODProperties.ScreenSize = 1.0f;

	ConfigureLODs({ LODProperties });

	SetupMaterialSlot(0, FName("Chunk Base"), WorldConfig.Material);

	FRuntimeMeshSectionProperties Properties0;
	Properties0.bCastsShadow = true;
	Properties0.bIsVisible = true;
	Properties0.MaterialSlot = 0;
	Properties0.bWants32BitIndices = false;
	Properties0.UpdateFrequency = ERuntimeMeshUpdateFrequency::Infrequent;
	
	CreateSection(0, 0, Properties0);


	MarkCollisionDirty();
}

FBoxSphereBounds URuntimeMeshProviderChunk::GetBounds()
{
	const FVector extend = Chunk->ChunkConfig.WorldConfig.GetChunkWorldSize();
	return FBoxSphereBounds(FBox(-extend*0.5f, (extend*0.5f)).ShiftBy(FVector(0,0,extend.Z/2)));
}

TArray<bool> URuntimeMeshProviderChunk::GetSidesToRender(FIntVector tilePosition) const
{
	TArray<bool> SidesToRender = {true, true, true, true, true, true};
	if(Chunk == nullptr) return SidesToRender;
	const TMap<FIntVector, FTile> Tiles = Chunk->GetTiles();
	if(Tiles.Find(FIntVector(tilePosition.X, tilePosition.Y, tilePosition.Z+1))) // Top
	{
		SidesToRender[0] = false;
	}
	if(Tiles.Find(FIntVector(tilePosition.X, tilePosition.Y, tilePosition.Z-1))) // Bottom
	{
		SidesToRender[1] = false;
	}
	if(Tiles.Find(FIntVector(tilePosition.X, tilePosition.Y+1, tilePosition.Z))) // Front
	{
		SidesToRender[2] = false;
	}
	if(Tiles.Find(FIntVector(tilePosition.X-1, tilePosition.Y, tilePosition.Z))) // Left
	{
		SidesToRender[3] = false;
	}
	if(Tiles.Find(FIntVector(tilePosition.X, tilePosition.Y-1, tilePosition.Z))) // Back
	{
		SidesToRender[4] = false;
	}
	if(Tiles.Find(FIntVector(tilePosition.X+1, tilePosition.Y, tilePosition.Z))) // Right
	{
		SidesToRender[5] = false;
	}
	return SidesToRender;
}

bool URuntimeMeshProviderChunk::GetSectionMeshForLOD(const int32 LODIndex, const int32 SectionId, FRuntimeMeshRenderableMeshData& MeshData)
{
	check(SectionId == 0 && LODIndex <= 1);
	SCOPE_CYCLE_COUNTER(STAT_GenerateMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateMesh, FColor::Green);
	FScopeLock Lock(&PropertySyncRoot);

	
	if(Chunk == nullptr) return false;

	TMap<FVector, int32> IndexLookup;
	for (const TTuple<FIntVector, FTile> tile : Chunk->GetTiles())
	{
		if(bMarkedForDestroy)
		{
			break;
		}
		if(	tile.Key.X < 0 || tile.Key.X >= Chunk->ChunkConfig.WorldConfig.ChunkSize.X ||
			tile.Key.Y < 0 || tile.Key.Y >= Chunk->ChunkConfig.WorldConfig.ChunkSize.Y ||
			tile.Key.Z < 0 || tile.Key.Z >= Chunk->ChunkConfig.WorldConfig.ChunkSize.Z ) 
		{
			continue;
		}
		const FIntVector Position  = tile.Key;
		const FTileConfig tileConfig = FTileConfig(GetSidesToRender(Position), tile.Value, Position);
		AddTile(MeshData, tileConfig);
	}
	if(MeshData.Triangles.Num() <= 0 || MeshData.Positions.Num() <= 0)
	{
		return false;
	}
	return true;
}

void URuntimeMeshProviderChunk::AddTile(FRuntimeMeshRenderableMeshData& MeshData, FTileConfig InTileConfig, bool isFlatShaded, TMap<FVector, int32>* IndexLookup)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPE_CYCLE_COUNTER(STAT_GenerateTileMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateTileMesh, FColor::Green);
	auto AddVertex = [&](const FVector& InPosition, const FVector& InTangentX, const FVector& InTangentZ, const FVector2f& InTexCoord, const FColor& InColor = FColor::White)
	{
		int32 index;
		if(isFlatShaded || IndexLookup == nullptr)
		{
			index = MeshData.Positions.Add(FVector3f(InPosition));
			MeshData.Tangents.Add(FVector3f(InTangentZ), FVector3f(InTangentX));
			MeshData.Colors.Add(InColor);
			MeshData.TexCoords.Add(InTexCoord, 0);
			return index;
		}else
		{
			if(const int* foundIndex = IndexLookup->Find(InPosition); foundIndex != nullptr && *foundIndex >= 0 && *foundIndex < MeshData.Positions.Num() )
			{
				index = *foundIndex;
				MeshData.Tangents.SetNormal(index, (MeshData.Tangents.GetNormal(index)+FVector3f(InTangentZ))/2);
			}
			else
			{
				index = MeshData.Positions.Add(FVector3f(InPosition));
				MeshData.Tangents.Add(FVector3f(InTangentZ), FVector3f(InTangentX));
				MeshData.Colors.Add(InColor);
				MeshData.TexCoords.Add(InTexCoord, 0);
				IndexLookup->Add(InPosition, index);
			}
			return index;
		}
	};
	const auto& [ChunkSize, MaxChunksZ, MaxChunkRenderDistance, BlockSize, TileTypes, Material] = Chunk->ChunkConfig.WorldConfig;
	const FVector BlockVertices[12] = {
		FVector(0.0f,	 0.0f,		0.0f),
		FVector(BlockSize.X, 0.0f,		0.0f),
		FVector(BlockSize.X, BlockSize.Y,	0.0f),
		FVector(0.0f,	 BlockSize.Y,	0.0f),
		
		FVector(0.0f,	 0.0f,		BlockSize.Z),
		FVector(BlockSize.X, 0.0f,		BlockSize.Z),
		FVector(BlockSize.X, BlockSize.Y,	BlockSize.Z),
		FVector(0.0f,	 BlockSize.Y,	BlockSize.Z),
	};
	
	FVector TangentX, TangentZ;
	auto [TileType, bIsSolid, bSideDiffers, TextureId, SideTextureId, Color, SideColor] = TileTypes[InTileConfig.Tile.TileID];
	if(!bSideDiffers)
	{
		SideColor = Color;
		SideTextureId = TextureId;
	}

	int textureMapSize = 8;
	FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
	FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
	FVector2f sideTextureOffset = FVector2f(SideTextureId%textureMapSize, floor(SideTextureId/textureMapSize))*textureSize;
	
	FVector position = FVector(InTileConfig.Position.X, InTileConfig.Position.Y, InTileConfig.Position.Z)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
	
	if(InTileConfig.SidesTORender[0]) // Top
	{
		TangentZ = FVector(0.0f, 0.0f, 1.0f);
		TangentX = FVector(0.0f, -1.0f, 0.0f);
		const int idx0 = AddVertex(BlockVertices[7]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+textureOffset, Color);
		const int idx1 = AddVertex(BlockVertices[4]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+textureOffset, Color);
		const int idx2 = AddVertex(BlockVertices[5]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+textureOffset, Color);
		const int idx3 = AddVertex(BlockVertices[6]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+textureOffset, Color);

		MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
		MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	}
	
	if(InTileConfig.SidesTORender[1]) // Bottom
	{
		TangentZ = FVector(0.0f, 0.0f, -1.0f);
		TangentX = FVector(0.0f, 1.0f, 0.0f);
		const int idx0 = AddVertex(BlockVertices[0]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+textureOffset, Color);
		const int idx1 = AddVertex(BlockVertices[3]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+textureOffset, Color);
		const int idx2 = AddVertex(BlockVertices[2]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+textureOffset, Color);
		const int idx3 = AddVertex(BlockVertices[1]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+textureOffset, Color);

		MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
		MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	}
	
	if(InTileConfig.SidesTORender[2]) // Front
	{
		TangentZ = FVector(0.0f, 1.0f, 0.0f);
		TangentX = FVector(1.0f, 0.0f, 0.0f);
		const int idx0 = AddVertex(BlockVertices[3]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
		const int idx1 = AddVertex(BlockVertices[7]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx2 = AddVertex(BlockVertices[6]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx3 = AddVertex(BlockVertices[2]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);

		MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
		MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	}

	if(InTileConfig.SidesTORender[3]) // Left
	{
		TangentZ = FVector(-1.0f, 0.0f, 0.0f);
		TangentX = FVector(0.0f, 1.0f, 0.0f);
		const int idx0 = AddVertex(BlockVertices[0]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
		const int idx1 = AddVertex(BlockVertices[4]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx2 = AddVertex(BlockVertices[7]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx3 = AddVertex(BlockVertices[3]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);

		MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
		MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	}

	if(InTileConfig.SidesTORender[4]) // Back
	{
		TangentZ = FVector(0.0f, -1.0f, 0.0f);
		TangentX = FVector(-1.0f, 0.0f, 0.0f);
		const int idx0 = AddVertex(BlockVertices[1]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
		const int idx1 = AddVertex(BlockVertices[5]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx2 = AddVertex(BlockVertices[4]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx3 = AddVertex(BlockVertices[0]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);

		MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
		MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	}

	if(InTileConfig.SidesTORender[5]) // Right
	{
		TangentZ = FVector(1.0f, 0.0f, 0.0f);
		TangentX = FVector(0.0f, -1.0f, 0.0f);
		const int idx0 = AddVertex(BlockVertices[2]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
		const int idx1 = AddVertex(BlockVertices[6]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx2 = AddVertex(BlockVertices[5]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
		const int idx3 = AddVertex(BlockVertices[1]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);

		MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
		MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	}
}


FRuntimeMeshCollisionSettings URuntimeMeshProviderChunk::GetCollisionSettings()
{
	FRuntimeMeshCollisionSettings Settings;
	Settings.bUseAsyncCooking = true;
	Settings.bUseComplexAsSimple = true;
	return Settings;
}

bool URuntimeMeshProviderChunk::HasCollisionMesh()
{
	return true;
}

bool URuntimeMeshProviderChunk::GetCollisionMesh(FRuntimeMeshCollisionData& CollisionData)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPE_CYCLE_COUNTER(STAT_GenerateCollisionMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateCollisionMesh, FColor::Green);

	if(Chunk == nullptr) return false;
	TMap<FVector, int> IndexLookup;
	for (const TTuple<FIntVector, FTile> tile : Chunk->GetTiles())
	{
		if(bMarkedForDestroy)
		{
			break;
		}
		const FIntVector Position  = tile.Key;
		const FTileConfig tileConfig = FTileConfig(GetSidesToRender(Position), tile.Value, Position);
		AddCollisionTile(CollisionData, tileConfig, &IndexLookup);
	}
	// UE_LOG(LogTemp, Warning, TEXT("Chunk Collision"))
	if(CollisionData.Triangles.Num() <= 0 || CollisionData.Vertices.Num() <= 0)
	{
		return false;
	}
	return true;
}

void URuntimeMeshProviderChunk::AddCollisionTile(FRuntimeMeshCollisionData& CollisionData, FTileConfig InTileConfig, TMap<FVector, int>* IndexLookup)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPE_CYCLE_COUNTER(STAT_GenerateTileCollisionMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateTileCollisionMesh, FColor::Green);
	
	auto AddVertex = [&](const FVector& InPosition)
	{
		if(IndexLookup == nullptr)
			return CollisionData.Vertices.Add(FVector3f(InPosition));
		
		if(const int* foundIndex = IndexLookup->Find(InPosition); foundIndex != nullptr && *foundIndex >= 0 && *foundIndex < CollisionData.Vertices.Num() )
		{
			return *foundIndex;
		}
		const int index = CollisionData.Vertices.Add(FVector3f(InPosition));
		IndexLookup->Add(InPosition, index);
		return index;
	};

	const auto& [ChunkSize, MaxChunksZ, MaxChunkRenderDistance, BlockSize, TileTypes, Material] = Chunk->ChunkConfig.WorldConfig;
	const FVector BlockVertices[8] = {
		FVector(0.0f,	 0.0f,			BlockSize.Z),
		FVector(0.0f,	 -BlockSize.Y,	BlockSize.Z),
		FVector(BlockSize.X, -BlockSize.Y,	BlockSize.Z),
		FVector(BlockSize.X, 0.0f,			BlockSize.Z),

		FVector(0.0f,	 0.0f,			0.0f),
		FVector(0.0f,	 -BlockSize.Y,	0.0f),
		FVector(BlockSize.X, -BlockSize.Y,	0.0f),
		FVector(BlockSize.X, 0.0f,			0.0f),
	};
	
	const FVector position = FVector(InTileConfig.Position.X, InTileConfig.Position.Y, InTileConfig.Position.Z) - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
	
	if(InTileConfig.SidesTORender[0]) // Top
	{
		const int idx0 = AddVertex(BlockVertices[0]+position);
		const int idx1 = AddVertex(BlockVertices[1]+position);
		const int idx2 = AddVertex(BlockVertices[2]+position);
		const int idx3 = AddVertex(BlockVertices[3]+position);

		CollisionData.Triangles.Add(idx0, idx2, idx1);
		CollisionData.Triangles.Add(idx0, idx3, idx2);
	}
	
	if(InTileConfig.SidesTORender[1]) // Bottom
	{
		const int idx0 = AddVertex(BlockVertices[5]+position);
		const int idx1 = AddVertex(BlockVertices[4]+position);
		const int idx2 = AddVertex(BlockVertices[7]+position);
		const int idx3 = AddVertex(BlockVertices[6]+position);

		CollisionData.Triangles.Add(idx0, idx2, idx1);
		CollisionData.Triangles.Add(idx0, idx3, idx2);
	}
	
	if(InTileConfig.SidesTORender[2]) // Front
	{
		const int idx0 = AddVertex(BlockVertices[4]+position);
		const int idx1 = AddVertex(BlockVertices[0]+position);
		const int idx2 = AddVertex(BlockVertices[3]+position);
		const int idx3 = AddVertex(BlockVertices[7]+position);

		CollisionData.Triangles.Add(idx0, idx2, idx1);
		CollisionData.Triangles.Add(idx0, idx3, idx2);
	}

	if(InTileConfig.SidesTORender[3]) // Left
	{
		const int idx0 = AddVertex(BlockVertices[5]+position);
		const int idx1 = AddVertex(BlockVertices[1]+position);
		const int idx2 = AddVertex(BlockVertices[0]+position);
		const int idx3 = AddVertex(BlockVertices[4]+position);

		CollisionData.Triangles.Add(idx0, idx2, idx1);
		CollisionData.Triangles.Add(idx0, idx3, idx2);
	}

	if(InTileConfig.SidesTORender[4]) // Back
	{
		const int idx0 = AddVertex(BlockVertices[6]+position);
		const int idx1 = AddVertex(BlockVertices[2]+position);
		const int idx2 = AddVertex(BlockVertices[1]+position);
		const int idx3 = AddVertex(BlockVertices[5]+position);

		CollisionData.Triangles.Add(idx0, idx2, idx1);
		CollisionData.Triangles.Add(idx0, idx3, idx2);
	}

	if(InTileConfig.SidesTORender[5]) // Right
	{
		const int idx0 = AddVertex(BlockVertices[7]+position);
		const int idx1 = AddVertex(BlockVertices[3]+position);
		const int idx2 = AddVertex(BlockVertices[2]+position);
		const int idx3 = AddVertex(BlockVertices[6]+position);

		CollisionData.Triangles.Add(idx0, idx2, idx1);
		CollisionData.Triangles.Add(idx0, idx3, idx2);
	}
}

bool URuntimeMeshProviderChunk::IsThreadSafe()
{
	return true;
}
