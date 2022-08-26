// Fill out your copyright notice in the Description page of Project Settings.


#include "World/ChunkStorage.h"

#include <string>

#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UChunkStorage::UChunkStorage()
{
	StoragePath = FPaths::ProjectSavedDir();
}

bool UChunkStorage::SaveChunk(FIntVector InPosition, TMap<FIntVector, FBlock> InBlocks) const
{
	FBufferArchive archive = FBufferArchive();
	for (auto block : InBlocks)
	{
		archive << block.Key;
		archive << block.Value;
	}
	FString FilePath = StoragePath + FString::Format(TEXT("Map/{0};{1};{2}.chunk"), {InPosition.X, InPosition.Y, InPosition.Z});
	FFileHelper::SaveArrayToFile(archive, *FilePath);
	archive.FlushCache();
	archive.Empty();
	return true;
}

TOptional<TMap<FIntVector, FBlock>> UChunkStorage::LoadChunk(const FIntVector& InPosition) const
{
	FString FilePath = StoragePath + FString::Format(TEXT("Map/{0};{1};{2}.chunk"), {InPosition.X, InPosition.Y, InPosition.Z});

	FBufferArchive BinaryArray = FBufferArchive();
	if(!FFileHelper::LoadFileToArray(BinaryArray, *FilePath, FILEREAD_Silent)) return {};
	if(BinaryArray.Num() <= 0) return {};
	FMemoryReader FromBinary(BinaryArray, true);
	FromBinary.Seek(0);
	TMap<FIntVector, FBlock> Blocks;
	while (!FromBinary.AtEnd())
	{
		FIntVector pos;
		FBlock loadedBlock;
		FromBinary << pos;
		FromBinary << loadedBlock;
		Blocks.Add(pos, loadedBlock);
	}
	FObjectAndNameAsStringProxyArchive Ar(FromBinary, true);
	BinaryArray.Empty();
	return Blocks;
}
