// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Generator.h"


TMap<FIntVector, FTile> UGenerator::GenerateChunk(FChunkConfig ChunkConfig, TMap<FIntVector, FTile> InTiles)
{
	TMap<FIntVector, FTile> tiles;
	const uint16 MaxHeight = ChunkConfig.WorldConfig.GetWorldBlockHeight();
	bool hasGeometry = InTiles.Num() > 0 ? true : false;
	for (int X = -1; X < ChunkConfig.WorldConfig.ChunkSize.X+1; ++X)
	{
		for (int Y = -1; Y < ChunkConfig.WorldConfig.ChunkSize.Y+1; ++Y)
		{
			const FIntVector ChunkPosition = ChunkConfig.GetChunkPositionInTiles();
			for (int Z = -1; Z < ChunkConfig.WorldConfig.ChunkSize.Z+1; ++Z)
			{
				const FIntVector WorldPosition = FIntVector(X + ChunkPosition.X, Y + ChunkPosition.Y, Z + ChunkPosition.Z);
				if(auto tile = InTiles.Find(FIntVector(X,Y,Z)); tile != nullptr)
				{
					if(tile->TileID != 255)
					{
						tiles.Add(FIntVector(X,Y,Z), *tile);
						if(!hasGeometry && !(
						X < 0 || X >= ChunkConfig.WorldConfig.ChunkSize.X ||
						Y < 0 || Y >= ChunkConfig.WorldConfig.ChunkSize.Y ||
						Z < 0 || Z >= ChunkConfig.WorldConfig.ChunkSize.Z) )
						{
							hasGeometry = true;
						}
					}
				}
				else if(FTileOrNull tileOrNull = GetTile(WorldPosition, ChunkConfig.WorldConfig); tileOrNull.hasTile)
				{
					tiles.Add(FIntVector(X,Y,Z), *tileOrNull.GetTile());
					if(!hasGeometry && !(
						X < 0 || X >= ChunkConfig.WorldConfig.ChunkSize.X ||
						Y < 0 || Y >= ChunkConfig.WorldConfig.ChunkSize.Y ||
						Z < 0 || Z >= ChunkConfig.WorldConfig.ChunkSize.Z) )
					{
						hasGeometry = true;
					}
				}
			}
		}	
	}
	return hasGeometry ? tiles : TMap<FIntVector, FTile>();
}
