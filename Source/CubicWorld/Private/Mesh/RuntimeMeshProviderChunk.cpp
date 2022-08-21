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

FSides URuntimeMeshProviderChunk::GetSidesToRender(const FIntVector InPosition, const int divider) const
{
	FSides SidesToRender(true);
	const int maxBlocks = divider*divider*divider;
	const FIntVector ChunkSize = Chunk->ChunkConfig.WorldConfig.ChunkSize;
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y, InPosition.Z+divider), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Z == ChunkSize.Z - divider && blocks.Num() >= ceil(divider*divider)) // Top
	{
		SidesToRender.Top = false;
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y, InPosition.Z-divider), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Z == 0 && blocks.Num() >= ceil(divider*divider)) // Bottom
	{
		SidesToRender.Bottom = false;
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y+divider, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Y == ChunkSize.Y - divider && blocks.Num() >= ceil(divider*divider)) // Front
	{
		SidesToRender.Front = false;
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X-divider, InPosition.Y, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.X == 0 && blocks.Num() >= ceil(divider*divider)) // Left
	{
		SidesToRender.Left = false;
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y-divider, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Y == 0 && blocks.Num() >= ceil(divider*divider)) // Back
	{
		SidesToRender.Back = false;
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X+divider, InPosition.Y, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.X == ChunkSize.X - divider && blocks.Num() >= ceil(divider*divider)) // Right
	{
		SidesToRender.Right = false;
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

			FSides sides = GetSidesToRender(Position);
			const FBlockConfig tileConfig = FBlockConfig(sides, tile.Value, Position, Chunk->ChunkConfig.WorldConfig.BlockSize);
			AddTile(MeshData, tileConfig);
			
		}
	} else
	{
		const int divider = FMath::Pow(2.0f, LODIndex);
		const FWorldConfig WorldConfig = Chunk->ChunkConfig.WorldConfig;
		for (int Z = 0; Z < WorldConfig.ChunkSize.Z/divider; ++Z)
		{
			for (int Y = 0; Y < WorldConfig.ChunkSize.Y/divider; ++Y)
			{
				for (int X = 0; X < WorldConfig.ChunkSize.X/divider; ++X)
				{
					const FIntVector Position  = FIntVector(X*divider, Y*divider, Z*divider);
					if(auto block = GetBlocks(Position, divider); !block.IsEmpty())
					{
						FSides sides = GetSidesToRender(Position, divider);
						const FBlockConfig tileConfig = FBlockConfig(sides, block.Last(), Position, Chunk->ChunkConfig.WorldConfig.BlockSize * divider);
						AddTile(MeshData, tileConfig);
					}
				}
			}
		}
	}
	GreedyMesh(MeshData);
	if(MeshData.Triangles.Num() <= 0 || MeshData.Positions.Num() <= 0)
	{
		return false;
	}
	return true;
}

void URuntimeMeshProviderChunk::AddTile(FRuntimeMeshRenderableMeshData &MeshData, const FBlockConfig& InTileConfig)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPE_CYCLE_COUNTER(STAT_GenerateTileMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateTileMesh, FColor::Green);
	auto AddVertex = [&](const FVector& InPosition, const FVector& InTangentX, const FVector& InTangentZ, const FVector2f& InTexCoord, const FColor& InColor = FColor::White)
	{
		const int32 index = MeshData.Positions.Add(FVector3f(InPosition));
		MeshData.Tangents.Add(FVector3f(InTangentZ), FVector3f(InTangentX));
		MeshData.Colors.Add(InColor);
		MeshData.TexCoords.Add(InTexCoord, 0);
		return index;
	};


	// BlockSize = Chunk->ChunkConfig.WorldConfig.BlockSize;
	
	const FVector BlockVertices[8] = {
		FVector(0.0f,	 0.0f,		0.0f),
		FVector(InTileConfig.Size.X, 0.0f,		0.0f),
		FVector(InTileConfig.Size.X, InTileConfig.Size.Y,	0.0f),
		FVector(0.0f,	 InTileConfig.Size.Y,	0.0f),
		
		FVector(0.0f,	 0.0f,		InTileConfig.Size.Z),
		FVector(InTileConfig.Size.X, 0.0f,		InTileConfig.Size.Z),
		FVector(InTileConfig.Size.X, InTileConfig.Size.Y,	InTileConfig.Size.Z),
		FVector(0.0f,	 InTileConfig.Size.Y,	InTileConfig.Size.Z),
	};

	const TArray<FTileType>& TileTypes = Chunk->ChunkConfig.WorldConfig.TileTypes;
	FVector TangentX, TangentZ;
	auto [TileType, bIsSolid, bSideDiffers, TextureId, SideTextureId, Color, SideColor] = TileTypes[InTileConfig.Tile.TileID];
	if(!bSideDiffers)
	{
		SideColor = Color;
		SideTextureId = TextureId;
	}

	// int textureMapSize = 8;
	// FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
	// FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
	// FVector2f sideTextureOffset = FVector2f(SideTextureId%textureMapSize, floor(SideTextureId/textureMapSize))*textureSize;
	//
	// FVector position = FVector(InTileConfig.Position)*Chunk->ChunkConfig.WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
	
	// if(InTileConfig.SidesTORender.Top) // Top
	// {
	// 	TangentZ = FVector(0.0f, 0.0f, 1.0f);
	// 	TangentX = FVector(0.0f, -1.0f, 0.0f);
	// 	const int idx0 = AddVertex(BlockVertices[7]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+textureOffset, Color);
	// 	const int idx1 = AddVertex(BlockVertices[4]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+textureOffset, Color);
	// 	const int idx2 = AddVertex(BlockVertices[5]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+textureOffset, Color);
	// 	const int idx3 = AddVertex(BlockVertices[6]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+textureOffset, Color);
	//
	// 	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	// 	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	// }
	//
	// if(InTileConfig.SidesTORender.Bottom) // Bottom
	// {
	// 	TangentZ = FVector(0.0f, 0.0f, -1.0f);
	// 	TangentX = FVector(0.0f, 1.0f, 0.0f);
	// 	const int idx0 = AddVertex(BlockVertices[0]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+textureOffset, Color);
	// 	const int idx1 = AddVertex(BlockVertices[3]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+textureOffset, Color);
	// 	const int idx2 = AddVertex(BlockVertices[2]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+textureOffset, Color);
	// 	const int idx3 = AddVertex(BlockVertices[1]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+textureOffset, Color);
	//
	// 	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	// 	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	// }
	//
	// if(InTileConfig.SidesTORender.Front) // Front
	// {
	// 	TangentZ = FVector(0.0f, 1.0f, 0.0f);
	// 	TangentX = FVector(1.0f, 0.0f, 0.0f);
	// 	const int idx0 = AddVertex(BlockVertices[3]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx1 = AddVertex(BlockVertices[7]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx2 = AddVertex(BlockVertices[6]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx3 = AddVertex(BlockVertices[2]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	//
	// 	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	// 	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	// }

	// if(InTileConfig.SidesTORender.Left) // Left
	// {
	// 	TangentZ = FVector(-1.0f, 0.0f, 0.0f);
	// 	TangentX = FVector(0.0f, 1.0f, 0.0f);
	// 	const int idx0 = AddVertex(BlockVertices[0]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx1 = AddVertex(BlockVertices[4]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx2 = AddVertex(BlockVertices[7]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx3 = AddVertex(BlockVertices[3]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	//
	// 	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	// 	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	// }

	// if(InTileConfig.SidesTORender.Back) // Back
	// {
	// 	TangentZ = FVector(0.0f, -1.0f, 0.0f);
	// 	TangentX = FVector(-1.0f, 0.0f, 0.0f);
	// 	const int idx0 = AddVertex(BlockVertices[1]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx1 = AddVertex(BlockVertices[5]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx2 = AddVertex(BlockVertices[4]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx3 = AddVertex(BlockVertices[0]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	//
	// 	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	// 	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	// }

	// if(InTileConfig.SidesTORender.Right) // Right
	// {
	// 	TangentZ = FVector(1.0f, 0.0f, 0.0f);
	// 	TangentX = FVector(0.0f, -1.0f, 0.0f);
	// 	const int idx0 = AddVertex(BlockVertices[2]+position, TangentX, TangentZ, FVector2f(0.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx1 = AddVertex(BlockVertices[6]+position, TangentX, TangentZ, FVector2f(0.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx2 = AddVertex(BlockVertices[5]+position, TangentX, TangentZ, FVector2f(1.0f, 0.0f)*textureSize+sideTextureOffset, Color);
	// 	const int idx3 = AddVertex(BlockVertices[1]+position, TangentX, TangentZ, FVector2f(1.0f, 1.0f)*textureSize+sideTextureOffset, Color);
	//
	// 	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	// 	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
	// }
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


void URuntimeMeshProviderChunk::GreedyMesh(FRuntimeMeshRenderableMeshData& MeshData)
{
	FScopeLock Lock(&PropertySyncRoot);
	auto AddVertex = [&](const FVector& InPosition, const FVector& InTangentX, const FVector& InTangentZ, const FVector2f& InTexCoord, const FColor& InColor = FColor::White)
	{
		const int32 index = MeshData.Positions.Add(FVector3f(InPosition));
		MeshData.Tangents.Add(FVector3f(InTangentZ), FVector3f(InTangentX));
		MeshData.Colors.Add(InColor);
		MeshData.TexCoords.Add(InTexCoord, 0);
		return index;
	};

	const FWorldConfig& WorldConfig = Chunk->ChunkConfig.WorldConfig;
	const FVector BlockVertices[8] = {
		FVector(0.0f,	 0.0f,		0.0f),												// 0 Left	Back	Bottom
		FVector(WorldConfig.BlockSize.X, 0.0f,		0.0f),									// 1 Right	Back	Bottom
		FVector(WorldConfig.BlockSize.X, WorldConfig.BlockSize.Y,	0.0f),						// 2 Right	Front	Bottom
		FVector(0.0f,	 WorldConfig.BlockSize.Y,	0.0f),									// 3 Left	Front	Bottom
		
		FVector(0.0f,	 0.0f,		WorldConfig.BlockSize.Z),								// 4 Left	Back	Top
		FVector(WorldConfig.BlockSize.X, 0.0f,		WorldConfig.BlockSize.Z),					// 5 Right	Back	Top
		FVector(WorldConfig.BlockSize.X, WorldConfig.BlockSize.Y,	WorldConfig.BlockSize.Z),		// 6 Right	Front	Top
		FVector(0.0f,	 WorldConfig.BlockSize.Y,	WorldConfig.BlockSize.Z),					// 7 Left	Front	Top
	};

	struct FState
	{
		FIntVector start;
		FIntVector end;
		bool active = false;
		bool generateSide = false;
		FTile block;
	};

	struct FQuadInfo
	{
		FVector Vertex1;
		FVector Vertex2;
		FVector Vertex3;
		FVector Vertex4;

		FIntVector Start;
		FIntVector End;
		
		FVector Normal;
		FVector Tangent;

		FColor Color;
		uint8 TextureId;

		FQuadInfo(const FVector& InVertex1, const FVector& InVertex2, const FVector& InVertex3, const FVector& InVertex4,
			const FIntVector& InStart, const FIntVector& InEnd,
			const FVector& InNormal, const FVector& InTangent, 
			const FColor& InColor, const uint8 InTextureId) :
				Vertex1(InVertex1), Vertex2(InVertex2), Vertex3(InVertex3), Vertex4(InVertex4),
				Start(InStart), End(InEnd),
				Normal(InNormal), Tangent(InTangent),
				Color(InColor), TextureId(InTextureId)
		{}
	};
	
	TArray<bool> visited;
	visited.Init(false, WorldConfig.GetMaxBlocks());


	int textureMapSize = 8;
	FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
	const TArray<FTileType>& TileTypes = Chunk->ChunkConfig.WorldConfig.TileTypes;
	
	for (int Z = 0; Z < WorldConfig.ChunkSize.Z; ++Z)
	{
		for (int Y = 0; Y < WorldConfig.ChunkSize.Y; ++Y)
		{
			FState Top, Bottom, Front, Back, Right, Left;
			for (int X = 0; X < WorldConfig.ChunkSize.X; ++X)
			{
				FIntVector BlockPosition(X,Y,Z);
				const FTile* block = Chunk->GetTiles().Find(BlockPosition);
				FSides sides = GetSidesToRender(BlockPosition);
				
				// top
				{
					auto GenerateQuad = [&](const FVector& Vertex1, const FVector& Vertex2, const FVector& Vertex3, const FVector& Vertex4,
						const FIntVector& Start, const FIntVector& End,
						const FVector& Normal, const FVector& Tangent, 
						const FColor& Color, const uint8 TextureId){
					
						FVector Position = FVector(Start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx0 = AddVertex(Vertex1+Position, Tangent, Normal, FVector2f(), Color);
						const int32 idx1 = AddVertex(Vertex2+Position, Tangent, Normal, FVector2f(), Color);
						Position = FVector(End)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx2 = AddVertex(Vertex3+Position, Tangent, Normal, FVector2f(), Color);
						const int32 idx3 = AddVertex(Vertex4+Position, Tangent, Normal, FVector2f(), Color);
						
						MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
						MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
						Top.generateSide = false;
					};
					
					if(Top.active && (!sides.Top || block == nullptr || Top.block != *block))
					{
						FTileType tileType = TileTypes[Top.block.TileID];
						FQuadInfo(BlockVertices[7], BlockVertices[4], BlockVertices[5], BlockVertices[6], Top.start, Top.end, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, tileType.Color, tileType.TextureId);
						GenerateQuad(BlockVertices[7], BlockVertices[4], BlockVertices[5], BlockVertices[6], Top.start, Top.end, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, tileType.Color, tileType.TextureId);
						Top.active = false;
					}
					if(block != nullptr && sides.Top)
					{
						if(!Top.active)
						{
							Top.start = {X,Y,Z};
							Top.block = *block;
							Top.active = true;
						}
						Top.end = {X,Y,Z};
					}
					if(Top.active && X >= WorldConfig.ChunkSize.X-1)
					{
						FTileType tileType = TileTypes[Top.block.TileID];
						GenerateQuad(BlockVertices[7], BlockVertices[4], BlockVertices[5], BlockVertices[6],  Top.start, Top.end, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, tileType.Color, tileType.TextureId);
						Top.active = false;
					}
				}

				// bottom
				{
					if(block != nullptr && sides.Bottom)
					{
						if(!Bottom.active)
						{
							Bottom.start = {X,Y,Z};
							Bottom.active = true;
						}
						Bottom.end = {X,Y,Z};
					} else
					{
						if(Bottom.active)
						{
							Bottom.generateSide = true;
							Bottom.active = false;
						}
					}

					if(X >= WorldConfig.ChunkSize.X-1 && Bottom.active)
					{
						Bottom.generateSide = true;
						Bottom.active = false;
					}

					if(Bottom.generateSide)
					{
						FVector TangentX(-1.0f, 0.0f, 0.0f);
						FVector TangentZ(0.0f, 0.0f, -1.0f);
						FColor Color = FColor::Orange;
						FVector Position = FVector(Bottom.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx0 = AddVertex(BlockVertices[0]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx1 = AddVertex(BlockVertices[3]+Position, TangentX, TangentZ, FVector2f(), Color);
						Position = FVector(Bottom.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx2 = AddVertex(BlockVertices[2]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx3 = AddVertex(BlockVertices[1]+Position, TangentX, TangentZ, FVector2f(), Color);
						
						MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
						MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
					
					}
				}

				// front
				{
					if(block != nullptr && sides.Front)
					{
						if(!Front.active)
						{
							Front.start = {X,Y,Z};
							Front.active = true;
						}
						Front.end = {X,Y,Z};
					} else
					{
						if(Front.active)
						{
							Front.generateSide = true;
							Front.active = false;
						}
					}

					if(X >= WorldConfig.ChunkSize.X-1 && Front.active)
					{
						Front.generateSide = true;
						Front.active = false;
					}

					if(Front.generateSide)
					{
						FVector TangentX(1.0f, 0.0f, 0.0f);
						FVector TangentZ(0.0f, 1.0f, 0.0f);
						FColor Color = FColor::Blue;
						FVector Position = FVector(Front.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx0 = AddVertex(BlockVertices[3]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx1 = AddVertex(BlockVertices[7]+Position, TangentX, TangentZ, FVector2f(), Color);
						Position = FVector(Front.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx2 = AddVertex(BlockVertices[6]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx3 = AddVertex(BlockVertices[2]+Position, TangentX, TangentZ, FVector2f(), Color);
						
						MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
						MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
					
					}
				}
				// back
				{
					if(block != nullptr && sides.Back)
					{
						if(!Back.active)
						{
							Back.start = {X,Y,Z};
							Back.active = true;
						}
						Back.end = {X,Y,Z};
					} else
					{
						if(Back.active)
						{
							Back.generateSide = true;
							Back.active = false;
						}
					}

					if(X >= WorldConfig.ChunkSize.X-1 && Back.active)
					{
						Back.generateSide = true;
						Back.active = false;
					}

					if(Back.generateSide)
					{
						FVector TangentX(-1.0f, 0.0f, 0.0f);
						FVector TangentZ(0.0f, -1.0f, 0.0f);
						FColor Color = FColor::Turquoise;
						FVector Position = FVector(Back.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx0 = AddVertex(BlockVertices[1]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx1 = AddVertex(BlockVertices[5]+Position, TangentX, TangentZ, FVector2f(), Color);
						Position = FVector(Back.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx2 = AddVertex(BlockVertices[4]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx3 = AddVertex(BlockVertices[0]+Position, TangentX, TangentZ, FVector2f(), Color);
						
						MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
						MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
					
					}
				}
				

				BlockPosition = FIntVector(Y,X,Z);

				block = Chunk->GetTiles().Find(BlockPosition);
				sides = GetSidesToRender(BlockPosition);

				// right
				{
					if(block != nullptr && sides.Right)
					{
						if(!Right.active)
						{
							Right.start = {Y,X,Z};
							Right.active = true;
						}
						Right.end = {Y,X,Z};
					} else
					{
						if(Right.active)
						{
							Right.generateSide = true;
							Right.active = false;
						}
					}

					if(X >= WorldConfig.ChunkSize.Y-1 && Right.active)
					{
						Right.generateSide = true;
						Right.active = false;
					}

					if(Right.generateSide)
					{
						FVector TangentX(0.0f, 1.0f, 0.0f);
						FVector TangentZ(1.0f, 0.0f, 0.0f);
						FColor Color = FColor::Magenta;
						FVector Position = FVector(Right.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx0 = AddVertex(BlockVertices[5]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx1 = AddVertex(BlockVertices[1]+Position, TangentX, TangentZ, FVector2f(), Color);
						Position = FVector(Right.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx2 = AddVertex(BlockVertices[2]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx3 = AddVertex(BlockVertices[6]+Position, TangentX, TangentZ, FVector2f(), Color);
						
						MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
						MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
					
					}
				}
				// left
				{
					if(block != nullptr && sides.Left)
					{
						if(!Left.active)
						{
							Left.start = {Y,X,Z};
							Left.active = true;
						}
						Left.end = {Y,X,Z};
					} else
					{
						if(Left.active)
						{
							Left.generateSide = true;
							Left.active = false;
						}
					}

					if(X >= WorldConfig.ChunkSize.Y-1 && Left.active)
					{
						Left.generateSide = true;
						Left.active = false;
					}

					if(Left.generateSide)
					{
						FVector TangentX(0.0f, -1.0f, 0.0f);
						FVector TangentZ(-1.0f, 0.0f, 0.0f);
						FColor Color = FColor::Green;
						FVector Position = FVector(Left.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx0 = AddVertex(BlockVertices[7]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx1 = AddVertex(BlockVertices[3]+Position, TangentX, TangentZ, FVector2f(), Color);
						Position = FVector(Left.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						const int32 idx2 = AddVertex(BlockVertices[0]+Position, TangentX, TangentZ, FVector2f(), Color);
						const int32 idx3 = AddVertex(BlockVertices[4]+Position, TangentX, TangentZ, FVector2f(), Color);
						
						MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
						MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
					
					}
				}
			}
		}	
	}
}

bool URuntimeMeshProviderChunk::IsThreadSafe()
{
	return true;
}
