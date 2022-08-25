﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Mesh/RuntimeMeshProviderChunk.h"
#include "Globals.h"

DECLARE_CYCLE_STAT(TEXT("Generate chunk mesh"), STAT_GenerateMesh, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Generate chunk collsion mesh"), STAT_GenerateCollisionMesh, STATGROUP_CubicWorld);

DECLARE_CYCLE_STAT(TEXT("Generate tile mesh"), STAT_GenerateTileMesh, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Generate tile collsion mesh"), STAT_GenerateTileCollisionMesh, STATGROUP_CubicWorld);


void URuntimeMeshProviderChunk::Initialize()
{
	FScopeLock Lock(&PropertySyncRoot);
	const FWorldConfig WorldConfig = Chunk->ChunkConfig.WorldConfig;
	UMaterialInterface* Material = WorldConfig.Material;
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
	
	BlockVertices = {
		FVector(0.0f,	 0.0f,		0.0f),												// 0 Left	Back	Bottom
		FVector(WorldConfig.BlockSize.X, 0.0f,		0.0f),									// 1 Right	Back	Bottom
		FVector(WorldConfig.BlockSize.X, WorldConfig.BlockSize.Y,	0.0f),						// 2 Right	Front	Bottom
		FVector(0.0f,	 WorldConfig.BlockSize.Y,	0.0f),									// 3 Left	Front	Bottom
		
		FVector(0.0f,	 0.0f,		WorldConfig.BlockSize.Z),								// 4 Left	Back	Top
		FVector(WorldConfig.BlockSize.X, 0.0f,		WorldConfig.BlockSize.Z),					// 5 Right	Back	Top
		FVector(WorldConfig.BlockSize.X, WorldConfig.BlockSize.Y,	WorldConfig.BlockSize.Z),		// 6 Right	Front	Top
		FVector(0.0f,	 WorldConfig.BlockSize.Y,	WorldConfig.BlockSize.Z),					// 7 Left	Front	Top
	};
}

FBoxSphereBounds URuntimeMeshProviderChunk::GetBounds()
{
	const FVector extend = Chunk->ChunkConfig.WorldConfig.GetChunkWorldSize();
	return FBoxSphereBounds(FBox(-extend*0.5f, (extend*0.5f)).ShiftBy(FVector(0,0,extend.Z/2)));
}

bool URuntimeMeshProviderChunk::GetSectionMeshForLOD(const int32 LODIndex, const int32 SectionId, FRuntimeMeshRenderableMeshData& MeshData)
{
	check(SectionId == 0);
	SCOPE_CYCLE_COUNTER(STAT_GenerateMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateMesh, FColor::Green);
	FScopeLock Lock(&PropertySyncRoot);
	
	if(Chunk == nullptr) return false;
	MeshData.TexCoords = FRuntimeMeshVertexTexCoordStream(2);

	// SimpleMesh(MeshData, 0);
	GreedyMesh(MeshData);

	if(MeshData.Triangles.Num() <= 0 || MeshData.Positions.Num() <= 0)
	{
		return false;
	}
	return true;
}

uint32 URuntimeMeshProviderChunk::AddVertex(FRuntimeMeshRenderableMeshData& MeshData, const FVector& InPosition,
	const FVector& InNormal, const FVector& InTangent,
	const FVector2f& InUV, const FVector2f& InTexCoord, const FColor& InColor)
{
	const int32 index = MeshData.Positions.Add(FVector3f(InPosition));
	MeshData.Tangents.Add(FVector3f(InNormal), FVector3f(InTangent));
	MeshData.Colors.Add(InColor);
	MeshData.TexCoords.Add({InUV, InTexCoord});
	return index;
}

void URuntimeMeshProviderChunk::AddQuad(FRuntimeMeshRenderableMeshData& MeshData, const FVector& Vertex1,
	const FVector& Vertex2, const FVector& Vertex3, const FVector& Vertex4,
	const FVector& Normal, const FVector& Tangent, const FVector2f TextureOffset,
	const FVector2f UVMultiplication, const FColor& Color)
{
	const int32 idx0 = AddVertex(MeshData, Vertex1, Normal, Tangent, FVector2f(0, 1)*UVMultiplication,	TextureOffset, Color);
	const int32 idx1 = AddVertex(MeshData, Vertex2, Normal, Tangent, FVector2f(0)*UVMultiplication, 		TextureOffset, Color);
	const int32 idx2 = AddVertex(MeshData, Vertex3, Normal, Tangent, FVector2f(1,0)*UVMultiplication,	TextureOffset, Color);
	const int32 idx3 = AddVertex(MeshData, Vertex4, Normal, Tangent, FVector2f(1)*UVMultiplication,	TextureOffset, Color);
					
	MeshData.Triangles.AddTriangle(idx0, idx2, idx1);
	MeshData.Triangles.AddTriangle(idx0, idx3, idx2);
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

void URuntimeMeshProviderChunk::GreedyMesh(FRuntimeMeshRenderableMeshData& MeshData)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateGreedyMesh, FColor::Cyan);

	
	const auto& [ChunkSize, MaxChunksZ, MaxChunkRenderDistance, BlockSize, TileTypes, Material, LODs] = Chunk->ChunkConfig.WorldConfig;

	struct FState
	{
		FIntVector start;
		FIntVector end;
		bool active = false;
		bool generateSide = false;
		FTile block;
		TMap<FIntVector, bool> visited;
		FState()
		{
		}
	};

	struct FBlockData
	{
		const FTile* Block;
		FIntVector Position;
		FSides Sides;

		FBlockData(): Block(nullptr), Position{0}, Sides{false}{}
		FBlockData(const FTile* const InBlock, const FIntVector& InPosition, const FSides& InSides) :
			Block(InBlock),
			Position(InPosition),
		    Sides(InSides)
		{
		}
	};

	auto GetGreedyTile = [&](FState& State, const FBlockData BlockData, const FSides::ESide Side)
	{
		int32  ChunkEndA = 0, ChunkEndB = 0;
		switch (Side)
		{
			case FSides::Top:
			case FSides::Bottom:
				ChunkEndA = ChunkSize.X - BlockData.Position.X;
				ChunkEndB = ChunkSize.Y - BlockData.Position.Y;
				break;
			case FSides::Front:
			case FSides::Back:
				ChunkEndA = ChunkSize.X - BlockData.Position.X;
				ChunkEndB = ChunkSize.Z - BlockData.Position.Z;
				break;
				
			case FSides::Right:
			case FSides::Left:
				ChunkEndA = ChunkSize.Y - BlockData.Position.Y;
				ChunkEndB = ChunkSize.Z - BlockData.Position.Z;
			break;
		}
		
		if(BlockData.Block != nullptr && BlockData.Sides.HasSide(Side) && !State.visited.Find(BlockData.Position))
		{
			State.active = true;
			State.block = *BlockData.Block;
			State.start = BlockData.Position;
			State.end = BlockData.Position;
			FIntVector tempEnd = BlockData.Position;
			int BreakIndex = MAX_int32;
			for (int B = 0; B < ChunkEndB && State.active; ++B)
			{
				for (int A = 0; A < ChunkEndA && State.active; ++A)
				{
					if(A == 0 && B == 0)
					{
						continue;
					} 
					if(B >= 1 && BreakIndex == MAX_int32)
					{
						BreakIndex = ChunkEndA;
						State.end = tempEnd;
					}
					FIntVector BlockPosition;
					switch (Side)
					{
						case FSides::Top:
						case FSides::Bottom:
							BlockPosition = {BlockData.Position.X+A, BlockData.Position.Y+B, BlockData.Position.Z};
							break;
						case FSides::Front:
						case FSides::Back:
							BlockPosition = {BlockData.Position.X+A, BlockData.Position.Y, BlockData.Position.Z+B};
							break;
				
						case FSides::Right:
						case FSides::Left:
							BlockPosition = {BlockData.Position.X, BlockData.Position.Y+A, BlockData.Position.Z+B};
							break;
					}
					const FTile* Block = Chunk->GetTiles().Find(BlockPosition);
					const FSides Sides = GetSidesToRender(BlockPosition);
					if(Block != nullptr && *Block == State.block && Sides.HasSide(Side) && !State.visited.Find(BlockPosition))
					{
						tempEnd = BlockPosition;
						if(B >= ChunkEndB-1 && (A >= ChunkEndA-1 || A >= BreakIndex-1))
						{
							State.end = tempEnd;
						}
					} else
					{
						if(BreakIndex == MAX_int32)
						{
							BreakIndex = A;
							State.end = tempEnd;
						} else
						{
							State.active = false;
							break;
						}
					}
					if(A >= BreakIndex-1)
					{
						State.end = tempEnd;
						break;
					}
				}
			}
			State.generateSide = true;
		}
	};
	
	FState Top, Bottom, Front, Back, Right, Left;
	Top.generateSide = false;
	Bottom.generateSide = false;
	Front.generateSide = false;
	Back.generateSide = false;
	Right.generateSide = false;
	Left.generateSide = false;
	for (int Z = 0; Z < ChunkSize.Z; ++Z)
	{
		for (int Y = 0; Y < ChunkSize.Y; ++Y)
		{
			for (int X = 0; X < ChunkSize.X; ++X)
			{
				FIntVector BlockPosition(X,Y,Z);
				FBlockData BlockData(Chunk->GetTiles().Find(BlockPosition), BlockPosition, GetSidesToRender(BlockPosition));
				
				// Top
				GetGreedyTile(Top, BlockData, FSides::Top);
				if(Top.generateSide)
				{
					int YsMax = Top.end.Y - Top.start.Y;
					int XsMax = Top.end.X - Top.start.X;
					for (int Ys = 0; Ys <= YsMax; ++Ys)
					{
						for (int Xs = 0; Xs <= XsMax; ++Xs)
						{
							Top.visited.Add(FIntVector(X+Xs, Y+Ys, Z), true);
						}
					}
					auto [TileType, bIsSolid, bSideDiffers, TextureId, SideTextureId, BottomTextureId, Color, SideColor] = TileTypes[Top.block.TileID];
					FVector PositionStart = FVector(Top.start)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Top.end)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Top.end-Top.start) + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Y).GetAbs();
					int textureMapSize = 8;
					if(!bSideDiffers)
					{
						SideTextureId = TextureId;
						BottomTextureId = TextureId;
						SideColor = Color;
						
					}
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
					AddQuad(MeshData,
						BlockVertices[7] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
						BlockVertices[4] + PositionStart,
						BlockVertices[5] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
						BlockVertices[6] + PositionEnd,
						{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
						textureOffset, UVMultiplication,
						Color);
					Top.active = false;
					Top.generateSide = false;
				}

				// Bottom
				GetGreedyTile(Bottom, BlockData, FSides::Bottom);
				if(Bottom.generateSide)
				{
					int YsMax = Bottom.end.Y - Bottom.start.Y;
					int XsMax = Bottom.end.X - Bottom.start.X;
					for (int Ys = 0; Ys <= YsMax; ++Ys)
					{
						for (int Xs = 0; Xs <= XsMax; ++Xs)
						{
							Bottom.visited.Add(FIntVector(X+Xs, Y+Ys, Z), true);
						}
					}
					FTileType tileType = TileTypes[Bottom.block.TileID];
					FVector PositionStart = FVector(Bottom.start)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Bottom.end)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Bottom.end-Bottom.start) + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Y).GetAbs();
					int textureMapSize = 8;
					int TextureId = tileType.bSideDiffers ? tileType.BottomTextureId : tileType.TextureId;
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
					FColor Color = tileType.bSideDiffers ? tileType.SideColor : tileType.Color;
					AddQuad(MeshData,
						BlockVertices[0] + PositionStart,
						BlockVertices[3] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
						BlockVertices[2] + PositionEnd,
						BlockVertices[1] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
						{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f},
						textureOffset, UVMultiplication,
						Color);
					Bottom.active = false;
					Bottom.generateSide = false;
				}

				// Front
				GetGreedyTile(Front, BlockData, FSides::Front);
				if(Front.generateSide)
				{
					int ZsMax = Front.end.Z - Front.start.Z;
					int XsMax = Front.end.X - Front.start.X;
					for (int Zs = 0; Zs <= ZsMax; ++Zs)
					{
						for (int Xs = 0; Xs <= XsMax; ++Xs)
						{
							Front.visited.Add(FIntVector(X+Xs, Y, Z+Zs), true);
						}
					}
					FTileType tileType = TileTypes[Front.block.TileID];
					FVector PositionStart = FVector(Front.start)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Front.end)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Front.end-Front.start) + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Z).GetAbs();
					int textureMapSize = 8;
					int TextureId = tileType.bSideDiffers ? tileType.SideTextureId : tileType.TextureId;
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
					FColor Color = tileType.bSideDiffers ? tileType.SideColor : tileType.Color;
					AddQuad(MeshData,
						BlockVertices[3] + PositionStart,
						BlockVertices[7] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
						BlockVertices[6] + PositionEnd,
						BlockVertices[2] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
						{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
						textureOffset, UVMultiplication,
						Color);
					Front.active = false;
					Front.generateSide = false;
				}

				// Back
				GetGreedyTile(Back, BlockData, FSides::Back);
				if(Back.generateSide)
				{
					int ZsMax = Back.end.Z - Back.start.Z;
					int XsMax = Back.end.X - Back.start.X;
					for (int Zs = 0; Zs <= ZsMax; ++Zs)
					{
						for (int Xs = 0; Xs <= XsMax; ++Xs)
						{
							Back.visited.Add(FIntVector(X+Xs, Y, Z+Zs), true);
						}
					}
					FTileType tileType = TileTypes[Back.block.TileID];
					FVector PositionStart = FVector(Back.end)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Back.start)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Back.start-Back.end).GetAbs() + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Z);
					int textureMapSize = 8;
					int TextureId = tileType.bSideDiffers ? tileType.SideTextureId : tileType.TextureId;
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
					FColor Color = tileType.bSideDiffers ? tileType.SideColor : tileType.Color;
					AddQuad(MeshData,
						BlockVertices[1] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
						BlockVertices[5] + PositionStart,
						BlockVertices[4] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
						BlockVertices[0] + PositionEnd,
						{0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
						textureOffset, UVMultiplication,
						Color);
					Back.active = false;
					Back.generateSide = false;
				}

				// Right
				GetGreedyTile(Right, BlockData, FSides::Right);
				if(Right.generateSide)
				{
					int ZsMax = Right.end.Z - Right.start.Z;
					int YsMax = Right.end.Y - Right.start.Y;
					for (int Zs = 0; Zs <= ZsMax; ++Zs)
					{
						for (int Ys = 0; Ys <= YsMax; ++Ys)
						{
							Right.visited.Add(FIntVector(X, Y+Ys, Z+Zs), true);
						}
					}
					FTileType tileType = TileTypes[Right.block.TileID];
					FVector PositionStart = FVector(Right.start)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Right.end)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Right.end-Right.start).GetAbs() + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.Y, QuadSize.Z);
					int textureMapSize = 8;
					int TextureId = tileType.bSideDiffers ? tileType.SideTextureId : tileType.TextureId;
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
					FColor Color = tileType.bSideDiffers ? tileType.SideColor : tileType.Color;
					AddQuad(MeshData,
						BlockVertices[2] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
						BlockVertices[6] + PositionEnd,
						BlockVertices[5] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
						BlockVertices[1] + PositionStart,
						{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
						textureOffset, UVMultiplication,
						Color);
					Right.active = false;
					Right.generateSide = false;
				}

				// Left
				GetGreedyTile(Left, BlockData, FSides::Left);
				if(Left.generateSide)
				{
					int ZsMax = Left.end.Z - Left.start.Z;
					int YsMax = Left.end.Y - Left.start.Y;
					for (int Zs = 0; Zs <= ZsMax; ++Zs)
					{
						for (int Ys = 0; Ys <= YsMax; ++Ys)
						{
							Left.visited.Add(FIntVector(X, Y+Ys, Z+Zs), true);
						}
					}
					FTileType tileType = TileTypes[Left.block.TileID];
					FVector PositionStart = FVector(Left.start)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Left.end)*BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Left.end-Left.start) + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.Y, QuadSize.Z).GetAbs();
					int textureMapSize = 8;
					int TextureId = tileType.bSideDiffers ? tileType.SideTextureId : tileType.TextureId;
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
					FColor Color = tileType.bSideDiffers ? tileType.SideColor : tileType.Color;
					AddQuad(MeshData,
						BlockVertices[0] + PositionStart,
						BlockVertices[4] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
						BlockVertices[7] + PositionEnd,
						BlockVertices[3] + FVector(PositionEnd.X, PositionEnd.Y, PositionStart.Z),
						{-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
						textureOffset, UVMultiplication,
						Color);
					Left.active = false;
					Left.generateSide = false;
				}
			}
		}	
	}
}

void URuntimeMeshProviderChunk::GreedyMesh(FRuntimeMeshRenderableMeshData& MeshData, uint32 LODIndex)
{
	// TODO Implement
}

void URuntimeMeshProviderChunk::SimpleMesh(FRuntimeMeshRenderableMeshData& MeshData, uint32 LODIndex)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateSimpleMesh, FColor::Cyan);
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
		SidesToRender.SetSide(FSides::Top, true);
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y, InPosition.Z-divider), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Z == 0 && blocks.Num() >= ceil(divider*divider)) // Bottom
	{
		SidesToRender.SetSide(FSides::Bottom, true);
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y+divider, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Y == ChunkSize.Y - divider && blocks.Num() >= ceil(divider*divider)) // Front
	{
		SidesToRender.SetSide(FSides::Front, true);
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X-divider, InPosition.Y, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.X == 0 && blocks.Num() >= ceil(divider*divider)) // Left
	{
		SidesToRender.SetSide(FSides::Left, true);
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X, InPosition.Y-divider, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.Y == 0 && blocks.Num() >= ceil(divider*divider)) // Back
	{
		SidesToRender.SetSide(FSides::Back, true);
	}
	if(const auto blocks = GetBlocks(FIntVector(InPosition.X+divider, InPosition.Y, InPosition.Z), divider);
		blocks.Num() >= maxBlocks ||
		InPosition.X == ChunkSize.X - divider && blocks.Num() >= ceil(divider*divider)) // Right
	{
		SidesToRender.SetSide(FSides::Right, true);
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


void URuntimeMeshProviderChunk::AddTile(FRuntimeMeshRenderableMeshData &MeshData, const FBlockConfig& InTileConfig)
{
	FScopeLock Lock(&PropertySyncRoot);
	SCOPE_CYCLE_COUNTER(STAT_GenerateTileMesh);
	SCOPED_NAMED_EVENT(URuntimeMeshProviderChunk_GenerateTileMesh, FColor::Green);

	const TArray<FTileType>& TileTypes = Chunk->ChunkConfig.WorldConfig.TileTypes;
	auto [TileType, bIsSolid, bSideDiffers, TextureId, SideTextureId, BottomTextureId, Color, SideColor] = InTileConfig.Tile.TileID < TileTypes.Num() ? TileTypes[InTileConfig.Tile.TileID] : TileTypes[0];

	
	if(!bSideDiffers)
	{
		SideTextureId = TextureId;
		BottomTextureId = TextureId;
		SideColor = Color;
	}

	constexpr int textureMapSize = 8;
	const FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
	const FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
	const FVector2f sideTextureOffset = FVector2f(SideTextureId%textureMapSize, floor(SideTextureId/textureMapSize))*textureSize;
	const FVector2f bottomTextureOffset = FVector2f(BottomTextureId%textureMapSize, floor(BottomTextureId/textureMapSize))*textureSize;
	const FVector position = FVector(InTileConfig.Position)*Chunk->ChunkConfig.WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);

	

	// Top
	if(InTileConfig.SidesToRender.HasSide(FSides::Top)) 
	{
		AddQuad(MeshData,
			BlockVertices[7]+position,
			BlockVertices[4]+position,
			BlockVertices[5]+position,
			BlockVertices[6]+position,
			{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
			textureOffset, {1,1},
			Color
			);
	}

	// Bottom
	if(InTileConfig.SidesToRender.HasSide(FSides::Bottom)) 
	{
		AddQuad(MeshData,
			BlockVertices[0]+position,
			BlockVertices[3]+position,
			BlockVertices[2]+position,
			BlockVertices[1]+position,
			{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f},
			bottomTextureOffset, {1,1},
			SideColor
			);
	}

	// Front
	if(InTileConfig.SidesToRender.HasSide(FSides::Front)) 
	{
		AddQuad(MeshData,
			BlockVertices[3]+position,
			BlockVertices[7]+position,
			BlockVertices[6]+position,
			BlockVertices[2]+position,
			{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
			sideTextureOffset, {1,1},
			SideColor
			);
	}

	// Back
	if(InTileConfig.SidesToRender.HasSide(FSides::Back)) 
	{
		AddQuad(MeshData,
			BlockVertices[1]+position,
			BlockVertices[5]+position,
			BlockVertices[4]+position,
			BlockVertices[0]+position,
			{0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
			sideTextureOffset, {1,1},
			SideColor
			);
	}

	// Right
	if(InTileConfig.SidesToRender.HasSide(FSides::Right)) 
	{
		AddQuad(MeshData,
			BlockVertices[2]+position,
			BlockVertices[6]+position,
			BlockVertices[5]+position,
			BlockVertices[1]+position,
			{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
			sideTextureOffset, {1,1},
			SideColor
			);
	}

	// Left
	if(InTileConfig.SidesToRender.HasSide(FSides::Left)) 
	{
		AddQuad(MeshData,
			BlockVertices[0]+position,
			BlockVertices[4]+position,
			BlockVertices[7]+position,
			BlockVertices[3]+position,
			{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
			sideTextureOffset, {1,1},
			SideColor
			);
	}
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

bool URuntimeMeshProviderChunk::GetHasCollision() const
{
	FScopeLock Lock(&PropertySyncRoot);
	return bHasCollision;
}



const UChunk* URuntimeMeshProviderChunk::GetChunk() const
{
	return Chunk;
}

void URuntimeMeshProviderChunk::SetChunk(const UChunk* InChunk)
{
	Chunk = InChunk;
}