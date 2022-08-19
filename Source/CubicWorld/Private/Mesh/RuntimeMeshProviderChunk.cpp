// Fill out your copyright notice in the Description page of Project Settings.


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
	const auto Material = Chunk->ChunkConfig.WorldConfig.Material;
	SetupMaterialSlot(0, FName("Chunk Base"), Material);
	
	// Setup LODs
	const auto LODConfig = Chunk->ChunkConfig.WorldConfig.LODs;
	TArray<FRuntimeMeshLODProperties> LODs;
	for (const auto lod : LODConfig)
	{
		FRuntimeMeshLODProperties LODProperties;
		LODProperties.ScreenSize = lod;
		LODs.Add(LODProperties);
	}
	ConfigureLODs(LODs);

	// Setup sections
	for (int32 LODIndex = 0; LODIndex < LODConfig.Num(); LODIndex++)
	{
		FRuntimeMeshSectionProperties Properties;
		Properties.bCastsShadow = true;
		Properties.bIsVisible = true;
		Properties.MaterialSlot = 0;
		Properties.UpdateFrequency = ERuntimeMeshUpdateFrequency::Infrequent;
		CreateSection(LODIndex, 0, Properties);
	}
}

FBoxSphereBounds URuntimeMeshProviderChunk::GetBounds()
{
	const FVector extend = Chunk->ChunkConfig.WorldConfig.GetChunkWorldSize();
	return FBoxSphereBounds(FBox(-extend*0.5f, (extend*0.5f)).ShiftBy(FVector(0,0,extend.Z/2)));
}

TArray<bool> URuntimeMeshProviderChunk::GetSidesToRender(const FIntVector InPosition, const int divider) const
{
	TArray<bool> SidesToRender = {true, true, true, true, true, true};
	const int blocks = divider*divider*divider;
	if(GetBlocks(FIntVector(InPosition.X, InPosition.Y, InPosition.Z+divider), divider).Num() >= blocks) // Top
		{
		SidesToRender[0] = false;
		}
	if(GetBlocks(FIntVector(InPosition.X, InPosition.Y, InPosition.Z-divider), divider).Num() >= blocks) // Bottom
		{
		SidesToRender[1] = false;
		}
	if(GetBlocks(FIntVector(InPosition.X, InPosition.Y+divider, InPosition.Z), divider).Num() >= blocks) // Front
		{
		SidesToRender[2] = false;
		}
	if(GetBlocks(FIntVector(InPosition.X-divider, InPosition.Y, InPosition.Z), divider).Num() >= blocks) // Left
		{
		SidesToRender[3] = false;
		}
	if(GetBlocks(FIntVector(InPosition.X, InPosition.Y-divider, InPosition.Z), divider).Num() >= blocks) // Back
		{
		SidesToRender[4] = false;
		}
	if(GetBlocks(FIntVector(InPosition.X+divider, InPosition.Y, InPosition.Z), divider).Num() >= blocks) // Right
		{
		SidesToRender[5] = false;
		}
	return SidesToRender;
}

TArray<FTile> URuntimeMeshProviderChunk::GetBlocks(const FIntVector InPosition, const int divider) const
{
	TArray<FTile> Tiles;
	for (int Z = 0; Z < divider; ++Z)
	{
		for (int Y = 0; Y < divider; ++Y)
		{
			for (int X = 0; X < divider; ++X)
			{
				if(const auto tile = Chunk->GetTiles().Find(InPosition+FIntVector(X,Y,Z)); tile != nullptr)
				{
					Tiles.Add(*tile);
				}
			}
		}
	}
	return Tiles;
}

bool URuntimeMeshProviderChunk::GetSectionMeshForLOD(const int32 LODIndex, const int32 SectionId, FRuntimeMeshRenderableMeshData& MeshData)
{
	check(SectionId == 0);
	SCOPE_CYCLE_COUNTER(STAT_GenerateMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateMesh, FColor::Green);
	FScopeLock Lock(&PropertySyncRoot);

	
	if(Chunk == nullptr) return false;

	// TMap<FVector, int32> IndexLookup;
	if(LODIndex == 0)
	{
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
			AddTile(MeshData, tileConfig, Chunk->ChunkConfig.WorldConfig.BlockSize);
			
		}
	} else
	{
		const int divider = FMath::Pow(2.0f, LODIndex);
		FWorldConfig WorldConfig = Chunk->ChunkConfig.WorldConfig;
		for (int Z = 0; Z < WorldConfig.ChunkSize.Z/divider; ++Z)
		{
			for (int Y = 0; Y < WorldConfig.ChunkSize.Y/divider; ++Y)
			{
				for (int X = 0; X < WorldConfig.ChunkSize.X/divider; ++X)
				{
					const FIntVector Position  = FIntVector(X*divider, Y*divider, Z*divider);
					if(auto block = GetBlocks(Position, divider); !block.IsEmpty())
					{
						const FTileConfig tileConfig = FTileConfig(GetSidesToRender(Position, divider), block.Last(), Position);
						FVector BlockSize = Chunk->ChunkConfig.WorldConfig.BlockSize * divider;
						AddTile(MeshData, tileConfig, BlockSize);
					}
				}
			}
		}
	}
	
	if(MeshData.Triangles.Num() <= 0 || MeshData.Positions.Num() <= 0)
	{
		return false;
	}
	return true;
}

void URuntimeMeshProviderChunk::AddTile(FRuntimeMeshRenderableMeshData &MeshData, FTileConfig InTileConfig, FVector BlockSize, bool isFlatShaded, TMap<FVector, int32> *IndexLookup)
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


	// BlockSize = Chunk->ChunkConfig.WorldConfig.BlockSize;
	
	const FVector BlockVertices[8] = {
		FVector(0.0f,	 0.0f,		0.0f),
		FVector(BlockSize.X, 0.0f,		0.0f),
		FVector(BlockSize.X, BlockSize.Y,	0.0f),
		FVector(0.0f,	 BlockSize.Y,	0.0f),
		
		FVector(0.0f,	 0.0f,		BlockSize.Z),
		FVector(BlockSize.X, 0.0f,		BlockSize.Z),
		FVector(BlockSize.X, BlockSize.Y,	BlockSize.Z),
		FVector(0.0f,	 BlockSize.Y,	BlockSize.Z),
	};

	const TArray<FTileType>& TileTypes = Chunk->ChunkConfig.WorldConfig.TileTypes;
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
	
	FVector position = FVector(InTileConfig.Position.X, InTileConfig.Position.Y, InTileConfig.Position.Z)*Chunk->ChunkConfig.WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
	
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
	return false;
}

bool URuntimeMeshProviderChunk::GetCollisionMesh(FRuntimeMeshCollisionData& CollisionData)
{
	return false;
}

bool URuntimeMeshProviderChunk::IsThreadSafe()
{
	return true;
}
