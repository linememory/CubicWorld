// Fill out your copyright notice in the Description page of Project Settings.


#include "World/ChunkStorage.h"

#include <string>

#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UChunkStorage::UChunkStorage()
{
	StoragePath = FPaths::ProjectSavedDir();
}

bool UChunkStorage::SaveChunk(const FIntVector& InPosition, const TChunkData& InBlocks) const
{
	FBufferArchive archive = FBufferArchive();
	FIntVector pos = InBlocks.GetChunkSize();
	archive << pos;

	FBlock BlockTmp = Air;
	uint32 blocks = 0;
	for(auto block : InBlocks)
	{
		if(blocks != 0 && BlockTmp != block)
		{
			archive << blocks;
			archive << BlockTmp;
			blocks = 0;
		}
		if(blocks == 0)
		{
			BlockTmp = block;
		}
		blocks++;
	}
	if(blocks != 0)
	{
		archive << blocks;
		archive << BlockTmp;
	}
	const FString name = FString::Format(TEXT("{0}{1}{2}.chunk"), {InPosition.X < 0 ? abs(InPosition.X)*2-1 : InPosition.X*2, InPosition.Y < 0 ? abs(InPosition.Y)*2-1 : InPosition.Y*2,InPosition.Z < 0 ? abs(InPosition.Z)*2-1 : InPosition.Z*2});
	const FString FilePath = StoragePath + "Map/" + name;
	FFileHelper::SaveArrayToFile(archive, *FilePath);
	archive.FlushCache();
	archive.Empty();
	return true;
}

TOptional<TChunkData> UChunkStorage::LoadChunk(const FIntVector& InPosition) const
{
	FString name = FString::Format(TEXT("{0}{1}{2}.chunk"), {InPosition.X < 0 ? abs(InPosition.X)*2-1 : InPosition.X*2, InPosition.Y < 0 ? abs(InPosition.Y)*2-1 : InPosition.Y*2,InPosition.Z < 0 ? abs(InPosition.Z)*2-1 : InPosition.Z*2});
	FString FilePath = StoragePath + "Map/" + name;
	FBufferArchive BinaryArray = FBufferArchive();
	TArray<FBlock> Blocks;
	if(!FFileHelper::LoadFileToArray(BinaryArray, *FilePath, FILEREAD_Silent)) return {};
	if(BinaryArray.Num() <= 0) return {};
	FMemoryReader FromBinary(BinaryArray, true);
	FromBinary.Seek(0);
	FIntVector ChunkSize(-1);
	while (!FromBinary.AtEnd())
	{
		if(ChunkSize == FIntVector(-1))
		{
			FromBinary << ChunkSize;
		}
		else
		{
			uint32 count;
			FBlock loadedBlock;
			FromBinary << count;
			FromBinary << loadedBlock;
			for (uint32 i = 0; i < count; ++i)
			{
				Blocks.Add(loadedBlock);
			}
		}
	}
	FObjectAndNameAsStringProxyArchive Ar(FromBinary, true);
	BinaryArray.Empty();
	return TChunkData(ChunkSize, Blocks);
}
