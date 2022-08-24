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

	
	const FWorldConfig& WorldConfig = Chunk->ChunkConfig.WorldConfig;

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

	const TArray<FTileType>& TileTypes = Chunk->ChunkConfig.WorldConfig.TileTypes;
	
	FState Top, Bottom, Front, Back, Right, Left;
	Top.generateSide = true;
	Bottom.generateSide = true;
	Front.generateSide = true;
	Back.generateSide = true;
	Right.generateSide = true;
	Left.generateSide = true;
	for (int Z = 0; Z < WorldConfig.ChunkSize.Z; ++Z)
	{
		for (int Y = 0; Y < WorldConfig.ChunkSize.Y; ++Y)
		{
			for (int X = 0; X < WorldConfig.ChunkSize.X; ++X)
			{
				FIntVector BlockPosition(X,Y,Z);
				const FTile* block = Chunk->GetTiles().Find(BlockPosition);
				FSides sides = GetSidesToRender(BlockPosition);

				//Top
				if(block != nullptr && sides.HasSide(FSides::Top) && !Top.visited.Find(BlockPosition))
				{
					FIntVector tempEnd;
					Top.active = true;
					Top.block = *block;
					Top.start = BlockPosition;
					Top.end = BlockPosition;
					tempEnd = BlockPosition;
					int xBreakIndex = MAX_int32;
					for (int Ys = 0; Ys < WorldConfig.ChunkSize.Y - Y && Top.active; ++Ys)
					{
						for (int Xs = 0; Xs < WorldConfig.ChunkSize.X - X && Top.active; ++Xs)
						{
							if(Xs == 0 && Ys == 0)
							{
								continue;
							} 
							if(Ys >= 1 && xBreakIndex == MAX_int32)
							{
								xBreakIndex = WorldConfig.ChunkSize.X - X;
								Top.end = tempEnd;
							}
							FIntVector BlockPosition2(X+Xs, Y+Ys, Z);
							const FTile* block2 = Chunk->GetTiles().Find(BlockPosition2);
							const FSides sides2 = GetSidesToRender(BlockPosition2);
							if(block2 != nullptr && *block2 == Top.block && sides2.HasSide(FSides::Top) && !Top.visited.Find(BlockPosition2))
							{
								tempEnd = BlockPosition2;
								if(Ys >= WorldConfig.ChunkSize.Y - Y-1 && (Xs >= WorldConfig.ChunkSize.X - X-1 || Xs >= xBreakIndex-1))
								{
									Top.end = tempEnd;
								}
							} else
							{
								if(xBreakIndex == MAX_int32)
								{
									xBreakIndex = Xs;
									Top.end = tempEnd;
								} else
								{
									Top.active = false;
									break;
								}
							}
							if(Xs >= xBreakIndex-1)
							{
								Top.end = tempEnd;
								break;
							}
						}
					}

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
					
						FTileType tileType = TileTypes[Top.block.TileID];
						FVector PositionStart = FVector(Top.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector PositionEnd = FVector(Top.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector QuadSize(FVector(Top.end-Top.start) + FVector(1));
						FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Y).GetAbs();
						int textureMapSize = 8;
						int TextureId = tileType.TextureId;
						FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
						FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
						AddQuad(MeshData,
							BlockVertices[7] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
							BlockVertices[4] + PositionStart,
							BlockVertices[5] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
							BlockVertices[6] + PositionEnd,
							{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
							textureOffset, UVMultiplication,
							tileType.Color);
						Top.active = false;
					}
				}

				//Bottom
				if(block != nullptr && sides.HasSide(FSides::Bottom) && !Bottom.visited.Find(BlockPosition))
				{
					FIntVector tempEnd;
					Bottom.active = true;
					Bottom.block = *block;
					Bottom.start = BlockPosition;
					Bottom.end = BlockPosition;
					tempEnd = BlockPosition;
					int xBreakIndex = MAX_int32;
					for (int Ys = 0; Ys < WorldConfig.ChunkSize.Y - Y && Bottom.active; ++Ys)
					{
						for (int Xs = 0; Xs < WorldConfig.ChunkSize.X - X && Bottom.active; ++Xs)
						{
							if(Xs == 0 && Ys == 0)
							{
								continue;
							} 
							if(Ys >= 1 && xBreakIndex == MAX_int32)
							{
								xBreakIndex = WorldConfig.ChunkSize.X - X;
								Bottom.end = tempEnd;
							}
							FIntVector BlockPosition2(X+Xs, Y+Ys, Z);
							const FTile* block2 = Chunk->GetTiles().Find(BlockPosition2);
							const FSides sides2 = GetSidesToRender(BlockPosition2);
							if(block2 != nullptr && *block2 == Bottom.block && sides2.HasSide(FSides::Bottom) && !Bottom.visited.Find(BlockPosition2))
							{
								tempEnd = BlockPosition2;
								if(Ys >= WorldConfig.ChunkSize.Y - Y-1 && (Xs >= WorldConfig.ChunkSize.X - X-1 || Xs >= xBreakIndex-1))
								{
									Bottom.end = tempEnd;
								}
							} else
							{
								if(xBreakIndex == MAX_int32)
								{
									xBreakIndex = Xs;
									Bottom.end = tempEnd;
								} else
								{
									Bottom.active = false;
									break;
								}
							}
							if(Xs >= xBreakIndex-1)
							{
								Bottom.end = tempEnd;
								break;
							}
						}
					}

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
						FVector PositionStart = FVector(Bottom.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector PositionEnd = FVector(Bottom.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector QuadSize(FVector(Bottom.end-Bottom.start) + FVector(1));
						FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Y).GetAbs();
						int textureMapSize = 8;
						int TextureId = tileType.TextureId;
						FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
						FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
						AddQuad(MeshData,
							BlockVertices[0] + PositionStart,
							BlockVertices[3] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
							BlockVertices[2] + PositionEnd,
							BlockVertices[1] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
							{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f},
							textureOffset, UVMultiplication,
							tileType.Color);
						Bottom.active = false;
					}
				}


				// Front
				if(block != nullptr && sides.HasSide(FSides::Front) && !Front.visited.Find(BlockPosition))
				{
					FIntVector tempEnd;
					Front.active = true;
					Front.block = *block;
					Front.start = BlockPosition;
					Front.end = BlockPosition;
					tempEnd = BlockPosition;
					int xBreakIndex = MAX_int32;
					for (int Zs = 0; Zs < WorldConfig.ChunkSize.Z - Z && Front.active; ++Zs)
					{
						for (int Xs = 0; Xs < WorldConfig.ChunkSize.X - X && Front.active; ++Xs)
						{
							if(Xs == 0 && Zs == 0)
							{
								continue;
							} 
							if(Zs >= 1 && xBreakIndex == MAX_int32)
							{
								xBreakIndex = WorldConfig.ChunkSize.X - X;
								Front.end = tempEnd;
							}
							FIntVector BlockPosition2(X+Xs, Y, Z+Zs);
							const FTile* block2 = Chunk->GetTiles().Find(BlockPosition2);
							const FSides sides2 = GetSidesToRender(BlockPosition2);
							if(block2 != nullptr && *block2 == Front.block && sides2.HasSide(FSides::Front) && !Front.visited.Find(BlockPosition2))
							{
								tempEnd = BlockPosition2;
								if(Zs >= WorldConfig.ChunkSize.Z - Z-1 && (Xs >= WorldConfig.ChunkSize.X - X-1 || Xs >= xBreakIndex-1))
								{
									Front.end = tempEnd;
								}
							} else
							{
								if(xBreakIndex == MAX_int32)
								{
									xBreakIndex = Xs;
									Front.end = tempEnd;
								} else
								{
									Front.active = false;
									break;
								}
							}
							if(Xs >= xBreakIndex-1)
							{
								Front.end = tempEnd;
								break;
							}
						}
					}
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
						FVector PositionStart = FVector(Front.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector PositionEnd = FVector(Front.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector QuadSize(FVector(Front.end-Front.start) + FVector(1));
						FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Z).GetAbs();
						int textureMapSize = 8;
						int TextureId = tileType.TextureId;
						FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
						FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
						AddQuad(MeshData,
							BlockVertices[3] + PositionStart,
							BlockVertices[7] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
							BlockVertices[6] + PositionEnd,
							BlockVertices[2] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
							{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
							textureOffset, UVMultiplication,
							tileType.Color);
						Front.active = false;
					}
				}

				// Back
				if(block != nullptr && sides.HasSide(FSides::Back) && !Back.visited.Find(BlockPosition))
				{
					FIntVector tempEnd;
					Back.active = true;
					Back.block = *block;
					Back.start = BlockPosition;
					Back.end = BlockPosition;
					tempEnd = BlockPosition;
					int xBreakIndex = MAX_int32;
					for (int Zs = 0; Zs < WorldConfig.ChunkSize.Z - Z && Back.active; ++Zs)
					{
						for (int Xs = 0; Xs < WorldConfig.ChunkSize.X - X && Back.active; ++Xs)
						{
							if(Xs == 0 && Zs == 0)
							{
								continue;
							} 
							if(Zs >= 1 && xBreakIndex == MAX_int32)
							{
								xBreakIndex = WorldConfig.ChunkSize.X - X;
								Back.end = tempEnd;
							}
							FIntVector BlockPosition2(X+Xs, Y, Z+Zs);
							const FTile* block2 = Chunk->GetTiles().Find(BlockPosition2);
							const FSides sides2 = GetSidesToRender(BlockPosition2);
							if(block2 != nullptr && *block2 == Back.block && sides2.HasSide(FSides::Back) && !Back.visited.Find(BlockPosition2))
							{
								tempEnd = BlockPosition2;
								if(Zs >= WorldConfig.ChunkSize.Z - Z-1 && (Xs >= WorldConfig.ChunkSize.X - X-1 || Xs >= xBreakIndex-1))
								{
									Back.end = tempEnd;
								}
							} else
							{
								if(xBreakIndex == MAX_int32)
								{
									xBreakIndex = Xs;
									Back.end = tempEnd;
								} else
								{
									Back.active = false;
									break;
								}
							}
							if(Xs >= xBreakIndex-1)
							{
								Back.end = tempEnd;
								break;
							}
						}
					}

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
						FVector PositionStart = FVector(Back.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector PositionEnd = FVector(Back.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector QuadSize(FVector(Back.start-Back.end).GetAbs() + FVector(1));
						FVector2f UVMultiplication = FVector2f(QuadSize.X, QuadSize.Z);
						int textureMapSize = 8;
						int TextureId = tileType.TextureId;
						FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
						FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
						AddQuad(MeshData,
							BlockVertices[1] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
							BlockVertices[5] + PositionStart,
							BlockVertices[4] + FVector(PositionEnd.X, PositionStart.Y, PositionStart.Z),
							BlockVertices[0] + PositionEnd,
							{0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
							textureOffset, UVMultiplication,
							tileType.Color);
						Back.active = false;
					}
				}

				// Right
				if(block != nullptr && sides.HasSide(FSides::Right) && !Right.visited.Find(BlockPosition))
				{
					FIntVector tempEnd;
					Right.active = true;
					Right.block = *block;
					Right.start = BlockPosition;
					Right.end = BlockPosition;
					tempEnd = BlockPosition;
					int yBreakIndex = MAX_int32;
					for (int Zs = 0; Zs < WorldConfig.ChunkSize.Z - Z && Right.active; ++Zs)
					{
						for (int Ys = 0; Ys < WorldConfig.ChunkSize.Y - Y && Right.active; ++Ys)
						{
							if(Ys == 0 && Zs == 0)
							{
								continue;
							} 
							if(Zs >= 1 && yBreakIndex == MAX_int32)
							{
								yBreakIndex = WorldConfig.ChunkSize.Y - Y;
								Right.end = tempEnd;
							}
							FIntVector BlockPosition2(X, Y+Ys, Z+Zs);
							const FTile* block2 = Chunk->GetTiles().Find(BlockPosition2);
							const FSides sides2 = GetSidesToRender(BlockPosition2);
							if(block2 != nullptr && *block2 == Right.block && sides2.HasSide(FSides::Right) && !Right.visited.Find(BlockPosition2))
							{
								tempEnd = BlockPosition2;
								if(Zs >= WorldConfig.ChunkSize.Z - Z-1 && (Ys >= WorldConfig.ChunkSize.Y - Y-1 || Ys >= yBreakIndex-1))
								{
									Right.end = tempEnd;
								}
							} else
							{
								if(yBreakIndex == MAX_int32)
								{
									yBreakIndex = Ys;
									Right.end = tempEnd;
								} else
								{
									Right.active = false;
									break;
								}
							}
							if(Ys >= yBreakIndex-1)
							{
								Right.end = tempEnd;
								break;
							}
						}
					}

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
						FVector PositionStart = FVector(Right.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector PositionEnd = FVector(Right.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector QuadSize(FVector(Right.end-Right.start).GetAbs() + FVector(1));
						FVector2f UVMultiplication = FVector2f(QuadSize.Y, QuadSize.Z);
						int textureMapSize = 8;
						int TextureId = tileType.TextureId;
						FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
						FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
						AddQuad(MeshData,
							BlockVertices[2] + FVector(PositionStart.X, PositionEnd.Y, PositionStart.Z),
							BlockVertices[6] + PositionEnd,
							BlockVertices[5] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
							BlockVertices[1] + PositionStart,
							{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
							textureOffset, UVMultiplication,
							tileType.Color);
						Right.active = false;
					}
				}

				// Left
				if(block != nullptr && sides.HasSide(FSides::Left) && !Left.visited.Find(BlockPosition))
				{
					FIntVector tempEnd;
					Left.active = true;
					Left.block = *block;
					Left.start = BlockPosition;
					Left.end = BlockPosition;
					tempEnd = BlockPosition;
					int yBreakIndex = MAX_int32;
					for (int Zs = 0; Zs < WorldConfig.ChunkSize.Z - Z && Left.active; ++Zs)
					{
						for (int Ys = 0; Ys < WorldConfig.ChunkSize.Y - Y && Left.active; ++Ys)
						{
							if(Ys == 0 && Zs == 0)
							{
								continue;
							} 
							if(Zs >= 1 && yBreakIndex == MAX_int32)
							{
								yBreakIndex = WorldConfig.ChunkSize.Y - Y;
								Left.end = tempEnd;
							}
							FIntVector BlockPosition2(X, Y+Ys, Z+Zs);
							const FTile* block2 = Chunk->GetTiles().Find(BlockPosition2);
							const FSides sides2 = GetSidesToRender(BlockPosition2);
							if(block2 != nullptr && *block2 == Left.block && sides2.HasSide(FSides::Left) && !Left.visited.Find(BlockPosition2))
							{
								tempEnd = BlockPosition2;
								if(Zs >= WorldConfig.ChunkSize.Z - Z-1 && (Ys >= WorldConfig.ChunkSize.Y - Y-1 || Ys >= yBreakIndex-1))
								{
									Left.end = tempEnd;
								}
							} else
							{
								if(yBreakIndex == MAX_int32)
								{
									yBreakIndex = Ys;
									Left.end = tempEnd;
								} else
								{
									Left.active = false;
									break;
								}
							}
							if(Ys >= yBreakIndex-1)
							{
								Left.end = tempEnd;
								break;
							}
						}
					}

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
						FVector PositionStart = FVector(Left.start)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector PositionEnd = FVector(Left.end)*WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);
						FVector QuadSize(FVector(Left.end-Left.start) + FVector(1));
						FVector2f UVMultiplication = FVector2f(QuadSize.Y, QuadSize.Z).GetAbs();
						int textureMapSize = 8;
						int TextureId = tileType.TextureId;
						FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
						FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
						AddQuad(MeshData,
							BlockVertices[0] + PositionStart,
							BlockVertices[4] + FVector(PositionStart.X, PositionStart.Y, PositionEnd.Z),
							BlockVertices[7] + PositionEnd,
							BlockVertices[3] + FVector(PositionEnd.X, PositionEnd.Y, PositionStart.Z),
							{-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
							textureOffset, UVMultiplication,
							tileType.Color);
						Left.active = false;
					}
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
	FVector Tangent, Normal;
	auto [TileType, bIsSolid, bSideDiffers, TextureId, SideTextureId, Color, SideColor] = InTileConfig.Tile.TileID < TileTypes.Num() ? TileTypes[InTileConfig.Tile.TileID] : TileTypes[0];
	if(!bSideDiffers)
	{
		SideColor = Color;
		SideTextureId = TextureId;
	}

	int textureMapSize = 8;
	FVector2f textureSize = FVector2f(1.0f/textureMapSize,1.0f/textureMapSize);
	FVector2f textureOffset = FVector2f(TextureId%textureMapSize, floor(TextureId/textureMapSize))*textureSize;
	FVector position = FVector(InTileConfig.Position)*Chunk->ChunkConfig.WorldConfig.BlockSize - FVector(GetBounds().BoxExtent.X,GetBounds().BoxExtent.Y,0.0f);

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
			textureOffset, {1,1},
			Color
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
			textureOffset, {1,1},
			Color
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
			textureOffset, {1,1},
			Color
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
			textureOffset, {1,1},
			Color
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
			textureOffset, {1,1},
			Color
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