// Fill out your copyright notice in the Description page of Project Settings.


#include "World/ChunkStorage.h"

#include <string>

#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UChunkStorage::UChunkStorage()
{
	StoragePath = FPaths::ProjectSavedDir();
}

bool UChunkStorage::SaveChunk(FIntVector InPosition, TMap<FIntVector, FTile> InTiles) const
{
	FBufferArchive archive = FBufferArchive();
	for (auto tile : InTiles)
	{
		archive << tile.Key;
		archive << tile.Value;
	}
	FString FilePath = StoragePath + FString::Format(TEXT("Map/{0};{1};{2}.chunk"), {InPosition.X, InPosition.Y, InPosition.Z});
	FFileHelper::SaveArrayToFile(archive, *FilePath);
	archive.FlushCache();
	archive.Empty();
	return true;
}

TOptional<TMap<FIntVector, FTile>> UChunkStorage::LoadChunk(const FIntVector& InPosition) const
{
	FString FilePath = StoragePath + FString::Format(TEXT("Map/{0};{1};{2}.chunk"), {InPosition.X, InPosition.Y, InPosition.Z});

	FBufferArchive BinaryArray = FBufferArchive();
	if(!FFileHelper::LoadFileToArray(BinaryArray, *FilePath, FILEREAD_Silent)) return {};
	if(BinaryArray.Num() <= 0) return {};
	FMemoryReader FromBinary(BinaryArray, true);
	FromBinary.Seek(0);
	TMap<FIntVector, FTile> Tiles;
	while (!FromBinary.AtEnd())
	{
		FIntVector pos;
		FTile tile;
		FromBinary << pos;
		FromBinary << tile;
		Tiles.Add(pos, tile);
	}
	FObjectAndNameAsStringProxyArchive Ar(FromBinary, true);
	BinaryArray.Empty();
	return Tiles;
}
