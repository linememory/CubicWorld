// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Generator.h"


void UGenerator::GenerateChunk(const FChunkConfig& ChunkConfig, TMap<FIntVector, FBlock>& Blocks)
{
	bool hasGeometry = Blocks.Num() > 0 ? true : false;
	for (int X = 0; X < ChunkConfig.WorldConfig.ChunkSize.X; ++X)
	{
		for (int Y = 0; Y < ChunkConfig.WorldConfig.ChunkSize.Y; ++Y)
		{
			const FIntVector ChunkPosition = ChunkConfig.GetChunkPositionInBlocks();
			for (int Z = 0; Z < ChunkConfig.WorldConfig.ChunkSize.Z; ++Z)
			{
				const FIntVector WorldPosition = FIntVector(X + ChunkPosition.X, Y + ChunkPosition.Y, Z + ChunkPosition.Z);
				if(const auto block = Blocks.Find(FIntVector(X,Y,Z)); block != nullptr)
				{
					if(!hasGeometry && !(
					X < 0 || X >= ChunkConfig.WorldConfig.ChunkSize.X ||
					Y < 0 || Y >= ChunkConfig.WorldConfig.ChunkSize.Y ||
					Z < 0 || Z >= ChunkConfig.WorldConfig.ChunkSize.Z) )
					{
						hasGeometry = true;
					}
				}
				else if(TOptional<FBlock> tileOrEmpty = GetTile(WorldPosition, ChunkConfig.WorldConfig); tileOrEmpty.IsSet())
				{
					Blocks.Add(FIntVector(X,Y,Z), tileOrEmpty.GetValue());
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
