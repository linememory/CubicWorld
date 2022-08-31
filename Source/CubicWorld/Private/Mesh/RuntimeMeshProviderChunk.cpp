// Fill out your copyright notice in the Description page of Project Settings.


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

	
	const FWorldConfig& WorldConfig = Chunk->ChunkConfig.WorldConfig;

	struct FState
	{
		FIntVector start;
		FIntVector end;
		bool active = false;
		bool generateSide = false;
		FBlock block;
		TMap<FIntVector, bool> visited;
		FState()
		{
		}
	};

	struct FBlockData
	{
		FBlock Block;
		FIntVector Position;
		FSides Sides;

		FBlockData(): Block(Air), Position{0}, Sides{false}{}
		FBlockData(FBlock InBlock, const FIntVector& InPosition, const FSides& InSides) :
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
				ChunkEndA = WorldConfig.ChunkSize.X - BlockData.Position.X;
				ChunkEndB = WorldConfig.ChunkSize.Y - BlockData.Position.Y;
				break;
			case FSides::Front:
			case FSides::Back:
				ChunkEndA = WorldConfig.ChunkSize.X - BlockData.Position.X;
				ChunkEndB = WorldConfig.ChunkSize.Z - BlockData.Position.Z;
				break;
				
			case FSides::Right:
			case FSides::Left:
				ChunkEndA = WorldConfig.ChunkSize.Y - BlockData.Position.Y;
				ChunkEndB = WorldConfig.ChunkSize.Z - BlockData.Position.Z;
			break;
		}
		
		if(BlockData.Block != Air && BlockData.Sides.HasSide(Side) && !State.visited.Find(BlockData.Position))
		{
			State.active = true;
			State.block = BlockData.Block;
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
					const FBlock Block = Chunk->GetBlock(BlockPosition);
					const FSides Sides = GetSidesToRender(BlockPosition);
					if(Block != Air && Block == State.block && Sides.HasSide(Side) && !State.visited.Find(BlockPosition))
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
	for (int Z = 0; Z < WorldConfig.ChunkSize.Z; ++Z)
	{
		for (int Y = 0; Y < WorldConfig.ChunkSize.Y; ++Y)
		{
			for (int X = 0; X < WorldConfig.ChunkSize.X; ++X)
			{
				FIntVector BlockPosition(X,Y,Z);
				FBlockData BlockData(Chunk->GetBlock(BlockPosition), BlockPosition, GetSidesToRender(BlockPosition));
				
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
					FBlockType tileType = WorldConfig.BlockTypes[Top.block.BlockTypeID];
					FVector PositionStart = FVector(Top.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Top.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector QuadSize(FVector(Top.end-Top.start) + FVector(1));
					FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Y).GetAbs();
					int textureMapSize = 8;
					FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
					FVector2f textureOffset = FVector2f(tileType.TextureId%textureMapSize, floor(tileType.TextureId/textureMapSize))*textureSize;
					AddQuad(MeshData,
						BlockVertices[7] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
						BlockVertices[4] + PositionStart,
						BlockVertices[5] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
						BlockVertices[6] + PositionEnd,
						{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
						textureOffset, UVMultiplication,
						tileType.Color);
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
					FBlockType tileType = WorldConfig.BlockTypes[Bottom.block.BlockTypeID];
					FVector PositionStart = FVector(Bottom.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Bottom.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
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
					FBlockType tileType = WorldConfig.BlockTypes[Front.block.BlockTypeID];
					FVector PositionStart = FVector(Front.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Front.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
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
					FBlockType tileType = WorldConfig.BlockTypes[Back.block.BlockTypeID];
					FVector PositionStart = FVector(Back.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Back.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
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
					FBlockType tileType = WorldConfig.BlockTypes[Right.block.BlockTypeID];
					FVector PositionStart = FVector(Right.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Right.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
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
					FBlockType tileType = WorldConfig.BlockTypes[Left.block.BlockTypeID];
					FVector PositionStart = FVector(Left.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
					FVector PositionEnd = FVector(Left.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
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

FSides URuntimeMeshProviderChunk::GetSidesToRender(const FIntVector InPosition) const
{
	FSides SidesToRender(true);
	const FIntVector ChunkSize = Chunk->ChunkConfig.WorldConfig.ChunkSize;
	if(const auto block = Chunk->GetBlock({InPosition.X, InPosition.Y, InPosition.Z+1});
		block != Air) // Top
	{
		SidesToRender.SetSide(FSides::Top, false);
	}
	if(const auto block = Chunk->GetBlock({InPosition.X, InPosition.Y, InPosition.Z-1});
		block != Air) // Bottom
	{
		SidesToRender.SetSide(FSides::Bottom, false);
	}
	if(const auto block = Chunk->GetBlock({InPosition.X, InPosition.Y+1, InPosition.Z});
		block != Air) // Front
	{
		SidesToRender.SetSide(FSides::Front, false);
	}
	if(const auto block = Chunk->GetBlock({InPosition.X-1, InPosition.Y, InPosition.Z});
		block != Air) // Left
	{
		SidesToRender.SetSide(FSides::Left, false);
	}
	if(const auto block = Chunk->GetBlock({InPosition.X, InPosition.Y-1, InPosition.Z});
		block != Air) // Back
	{
		SidesToRender.SetSide(FSides::Back, false);
	}
	if(const auto block = Chunk->GetBlock({InPosition.X+1, InPosition.Y, InPosition.Z});
		block != Air) // Right
	{
		SidesToRender.SetSide(FSides::Right, false);
	}
	return SidesToRender;
}

const UChunk* URuntimeMeshProviderChunk::GetChunk() const
{
	return Chunk;
}

void URuntimeMeshProviderChunk::SetChunk(const UChunk* InChunk)
{
	Chunk = InChunk;
}