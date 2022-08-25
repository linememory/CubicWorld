// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Generator.h"


void UGenerator::GenerateChunk(const FChunkConfig& ChunkConfig, TMap<FIntVector, FTile>& Tiles)
{
	bool hasGeometry = Tiles.Num() > 0 ? true : false;
	for (int X = -1; X < ChunkConfig.WorldConfig.ChunkSize.X+1; ++X)
	{
		for (int Y = -1; Y < ChunkConfig.WorldConfig.ChunkSize.Y+1; ++Y)
		{
			const FIntVector ChunkPosition = ChunkConfig.GetChunkPositionInTiles();
			for (int Z = -1; Z < ChunkConfig.WorldConfig.ChunkSize.Z+1; ++Z)
			{
				const FIntVector WorldPosition = FIntVector(X + ChunkPosition.X, Y + ChunkPosition.Y, Z + ChunkPosition.Z);
				if(const auto tile = Tiles.Find(FIntVector(X,Y,Z)); tile != nullptr)
				{
					if(!hasGeometry && !(
					X < 0 || X >= ChunkConfig.WorldConfig.ChunkSize.X ||
					Y < 0 || Y >= ChunkConfig.WorldConfig.ChunkSize.Y ||
					Z < 0 || Z >= ChunkConfig.WorldConfig.ChunkSize.Z) )
					{
						hasGeometry = true;
					}
				}
				else if(TOptional<FTile> tileOrEmpty = GetTile(WorldPosition, ChunkConfig.WorldConfig); tileOrEmpty.IsSet())
				{
					Tiles.Add(FIntVector(X,Y,Z), tileOrEmpty.GetValue());
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
}
