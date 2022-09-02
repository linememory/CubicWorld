// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Generator.h"


void UGenerator::GenerateChunk(const FChunkConfig& ChunkConfig, TChunkData& Blocks)
{
	for (int X = 0; X < ChunkConfig.WorldConfig.ChunkSize.X; ++X)
	{
		for (int Y = 0; Y < ChunkConfig.WorldConfig.ChunkSize.Y; ++Y)
		{
			const FIntVector ChunkPosition = ChunkConfig.GetChunkPositionInBlocks();
			for (int Z = 0; Z < ChunkConfig.WorldConfig.ChunkSize.Z; ++Z)
			{
				const FIntVector WorldPosition = FIntVector(X + ChunkPosition.X, Y + ChunkPosition.Y, Z + ChunkPosition.Z);
				if(Blocks.GetBlock(FIntVector(X,Y,Z)) == Air)
				{
					TOptional<FBlock> tileOrEmpty = GetTile(WorldPosition, ChunkConfig.WorldConfig);
					Blocks.SetBlock(FIntVector(X,Y,Z), tileOrEmpty.IsSet() ? tileOrEmpty.GetValue() : Air);
				}
			}
		}	
	}
}
