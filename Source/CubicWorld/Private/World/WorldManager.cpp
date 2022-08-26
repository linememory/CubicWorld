﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "World/WorldManager.h"

#include <algorithm>
#include <concrt.h>

#include "Globals.h"
#include "Kismet/GameplayStatics.h"
#include "Mesh/ChunkMesh.h"
#include "World/SimpleGenerator.h"

DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_Tick, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Update visible chunks"), STAT_UpdateVisibleChunks, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Generate chunks"), STAT_GenerateChunks, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Generate chunk meshes"), STAT_GenerateChunkMeshes, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("render chunks"), STAT_Render, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Unload chunk meshes"), STAT_UnloadChunks, STATGROUP_CubicWorld);
DECLARE_CYCLE_STAT(TEXT("Check chunks to unload"), STAT_CheckChunksToUnload, STATGROUP_CubicWorld);

AWorldManager::AWorldManager()
{
	GeneratorClass = USimpleGenerator::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1;
}

AWorldManager::~AWorldManager()
{
	UE_LOG(LogTemp, Warning, TEXT("Dispose"))
	if(GeneratorRunner != nullptr)
	{
		delete GeneratorRunner;
		GeneratorRunner = nullptr;
	}
	
}


void AWorldManager::BeginPlay()
{
	Super::BeginPlay();
	if(GeneratorClass !=  nullptr)
	{
		Generator = NewObject<UGenerator>(this, GeneratorClass);
	} else
	{
		Generator = NewObject<UGenerator>();
	}

	GeneratorRunner = new FGeneratorRunner(Generator, WorldConfig);

	ChunkStorage = NewObject<UChunkStorage>();
}

void AWorldManager::BeginDestroy()
{
	ChunksToLoad.Empty();
	ChunksToRebuild.Empty();
	ChunksToUnload.Empty();
	ChunkMeshesToGenerate.Empty();
	if(GeneratorRunner != nullptr)
	{
		GeneratorRunner->Stop();
		GeneratorRunner->Exit();
	}
	for (const auto chunkMesh : ChunkMeshes)
	{
		chunkMesh.Value->Destroy();
	}
	Super::BeginDestroy();
	UE_LOG(LogTemp, Warning, TEXT("BeginDestroy"))
}


void AWorldManager::Tick(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Tick);
	SCOPED_NAMED_EVENT(AWorldManager_Tick, FColor::Green);
	
	Super::Tick(DeltaTime);
	UpdateVisibleChunks();
	GenerateChunks();
	GenerateChunkMeshes();
	UnloadChunks();
	ChunksToLoad.Reset();

	MillisSinceLastSave += DeltaTime;
	if(MillisSinceLastSave >= 1000 * 10)
	{
		for (const auto modifiedChunk : ModifiedChunks)
		{
			
			if(ChunkStorage->SaveChunk(modifiedChunk.Key, modifiedChunk.Value.ModifiedBlocks))
			{
				UE_LOG(LogTemp, Error, TEXT("Could not save chunk to file"))
			}
		}
		MillisSinceLastSave = 0;
	}
}


FIntVector AWorldManager::WorldToLocalPosition(const FVector InPosition) const
{
	FVector pos =  InPosition/WorldConfig.GetChunkWorldSize();
	if(pos.Z < 0)
		pos.Z -= 1;
	
	return FIntVector(round(pos.X),round(pos.Y),round(pos.Z));
}

void AWorldManager::UpdateVisibleChunks()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateVisibleChunks);
	SCOPED_NAMED_EVENT(AWorldManager_UpdateVisibleChunks, FColor::Blue);
	for (const auto trackable : TrackerComponents)
	{
		if (trackable->bIsTrackable)
		{
			const uint8 distance = std::clamp(trackable->ChunkRenderDistance, static_cast<uint8>(1), WorldConfig.MaxChunkRenderDistance);
			const FIntVector position = WorldToLocalPosition(trackable->GetOwner()->GetActorLocation());

			if(trackable->LastWorldPosition != position)
				UE_LOG(LogTemp, Warning, TEXT("Position: %s"), *position.ToString());

			trackable->LastWorldPosition = position;
			for (int Z = 0; Z < WorldConfig.MaxChunksZ; ++Z)
			{
				for (int Y = -distance; Y <= distance; ++Y)
				{
					for (int X = -distance; X <= distance; ++X)
					{
						if(FIntVector chunkToLoad = FIntVector(position.X+X, position.Y+Y, Z);
							ChunksToLoad.Find(chunkToLoad) == INDEX_NONE && Chunks.Find(chunkToLoad) == nullptr)
						{
							ChunksToLoad.Add(chunkToLoad);
							ChunksToUnload.Remove(chunkToLoad);
							
						}
					}
				}	
			}
			auto SortPredicate = [&](const FIntVector& A, const FIntVector& B)->bool
			{
				const FVector Trackable = FVector(trackable->LastWorldPosition);
				const float ADist = FVector::Distance(Trackable, FVector(A));
				const float BDist = FVector::Distance(Trackable, FVector(B));
				return ADist < BDist;
			};
			ChunksToLoad.Sort(SortPredicate);
		}
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_CheckChunksToUnload);
		SCOPED_NAMED_EVENT(AWorldManager_CheckChunksToUnload, FColor::Blue);
		// Check chunks to unload
		for (auto chunk : Chunks)
		{
			bool shouldUnload = true;
			for (const auto trackable : TrackerComponents)
			{
				if(!trackable->bIsTrackable)break;
				
				const float distance = std::clamp(FVector(trackable->ChunkRenderDistance,trackable->ChunkRenderDistance,0).Length(), 1.0, FVector(WorldConfig.MaxChunkRenderDistance,WorldConfig.MaxChunkRenderDistance,0).Length());
				const FIntVector position = WorldToLocalPosition(trackable->GetOwner()->GetActorLocation());
					
				const float distanceToTrackable = FVector::Distance(FVector(position.X, position.Y, 0.0f), FVector(chunk.Key.X, chunk.Key.Y, 0.0f));
				if(distanceToTrackable <= distance+4)
				{
					shouldUnload = false;
				}
			}
			if(shouldUnload)
				ChunksToUnload.Add(chunk.Key);
		}
	}
}

void AWorldManager::GenerateChunks()
{
	SCOPE_CYCLE_COUNTER(STAT_GenerateChunks);
	SCOPED_NAMED_EVENT(AWorldManager_GenerateChunks, FColor::Blue);
	{
		SCOPED_NAMED_EVENT(AWorldManager_SendChunksToGeneratorRunner, FColor::Blue);
		for (auto chunkPosition : ChunksToLoad)
		{
			const FChunkConfig chunkConfig = FChunkConfig(WorldConfig, chunkPosition);
			UChunk* chunk = NewObject<UChunk>();
			chunk->ChunkConfig = chunkConfig;
			Chunks.Add(chunkPosition, chunk);

			auto tiles = ChunkStorage->LoadChunk(chunkPosition);
		
			if(tiles.IsSet())
			{
				ModifiedChunks.Add({chunkPosition, FModifiedChunk(tiles.GetValue())});
			}
			
			GeneratorRunner->Tasks.Enqueue({chunkPosition, tiles.IsSet() ? tiles.GetValue() : TMap<FIntVector, FBlock>()});
		}
	}

	{
		TPair<FIntVector, TMap<FIntVector, FBlock>> tiles;
		SCOPED_NAMED_EVENT(AWorldManager_GetGeneratedBlocks, FColor::Blue);
		while (GeneratorRunner->Results.Dequeue(tiles))
		{
			bool hasGeometry = !tiles.Value.IsEmpty();
			for (int Y = 0; Y < WorldConfig.ChunkSize.Y && hasGeometry; ++Y)
			{
				for (int X = 0; X < WorldConfig.ChunkSize.X && hasGeometry; ++X)
				{
					if(const FBlock* foundBlock = tiles.Value.Find(FIntVector(X, Y, WorldConfig.ChunkSize.Z)); foundBlock == nullptr)
					{
						hasGeometry = true;
					}
				}
			}
			if(UChunk** chunk = Chunks.Find(tiles.Key); chunk != nullptr && *chunk != nullptr)
			{
				(*chunk)->SetBlocks(tiles.Value);
				if(hasGeometry)
					ChunkMeshesToGenerate.Enqueue(*chunk);
			}
		}
	}
	ChunksToLoad.Reset();
}

void AWorldManager::GenerateChunkMeshes()
{
	SCOPE_CYCLE_COUNTER(STAT_GenerateChunkMeshes);
	SCOPED_NAMED_EVENT(AWorldManager_GenerateChunkMeshes, FColor::Blue);
	while (!ChunkMeshesToGenerate.IsEmpty())
	{
		UChunk* chunkToGenerate;
		ChunkMeshesToGenerate.Dequeue(chunkToGenerate);
		const FIntVector* Position = Chunks.FindKey(chunkToGenerate);
		AChunkMesh* ChunkMesh;
		if(const auto foundChunkMesh = ChunkMeshes.Find(*Position); foundChunkMesh != nullptr && *foundChunkMesh != nullptr)
		{
			ChunkMesh = *foundChunkMesh;
			
		} else
		{
			FVector SpawnLocation = FVector(FVector(*Position) * WorldConfig.GetChunkWorldSize());
			ChunkMesh =  GetWorld()->SpawnActor<AChunkMesh>(SpawnLocation, FRotator(0));
			ChunkMesh->Chunk = chunkToGenerate;
			ChunkMeshes.Add(*Position, ChunkMesh);
		}
		ChunkMesh->GenerateMesh();
	}
}

void AWorldManager::UnloadChunks()
{
	SCOPE_CYCLE_COUNTER(STAT_UnloadChunks);
	SCOPED_NAMED_EVENT(AWorldManager_UnloadChunks, FColor::Blue);
	for (auto position : ChunksToUnload)
	{
		if(const auto chunkMesh = ChunkMeshes.Find(position); chunkMesh != nullptr && *chunkMesh != nullptr)
		{
			(*chunkMesh)->Destroy();
			Chunks.Remove(position);
			ChunkMeshes.Remove(position);
		}
	}
	ChunksToUnload.Reset();
}

void AWorldManager::UpdateChunk(FIntVector InChunkPosition, const FIntVector InBlockPosition)
{
	if(const auto chunk = Chunks.Find(InChunkPosition); chunk != nullptr && *chunk != nullptr)
	{
		(*chunk)->RemoveBlock(InBlockPosition);
		ChunkMeshesToGenerate.Enqueue(*chunk);

		auto modifiedChunk = ModifiedChunks.Find(InChunkPosition);
		if(modifiedChunk == nullptr)
		{
			modifiedChunk = &ModifiedChunks.Add({InChunkPosition, FModifiedChunk()});
		}
		if(modifiedChunk == nullptr)
		{
			modifiedChunk = &ModifiedChunks.Add({InChunkPosition, FModifiedChunk()});
		}
		modifiedChunk->ModifiedBlocks.Add(InBlockPosition, FBlock(255));
	}
}


TArray<FBlockType> AWorldManager::GetBlockTypes() const
{
	return WorldConfig.BlockTypes;
}

void AWorldManager::SetBlockTypes(const TArray<FBlockType> InBlockTypes)
{
	WorldConfig.BlockTypes = InBlockTypes;
}

void AWorldManager::AddActorToTrack(AActor* InActorToTrack)
{
	UTrackable* trackable = NewObject<UTrackable>(InActorToTrack);
	trackable->RegisterComponent();
	UE_LOG(LogTemp, Warning, TEXT("Owner of trackable: %s"), *trackable->GetOwner()->GetName())
	TrackerComponents.Add(trackable);
}

FIntVector AWorldManager::GetChunkPositionFromBlockWorldCoordinates(const FIntVector BlockPosition) const
{
	return FIntVector(floor(static_cast<float>(BlockPosition.X)/WorldConfig.ChunkSize.X), floor(static_cast<float>(BlockPosition.Y)/WorldConfig.ChunkSize.Y), floor(static_cast<float>(BlockPosition.Z)/WorldConfig.ChunkSize.Z));
}

FIntVector AWorldManager::GetBlockPositionFromWorldBlockCoordinates(const FIntVector BlockPosition) const
{
	FIntVector blockPosition =  FIntVector(BlockPosition.X%WorldConfig.ChunkSize.X,BlockPosition.Y%WorldConfig.ChunkSize.Y,BlockPosition.Z%WorldConfig.ChunkSize.Z);
	blockPosition = FIntVector(	blockPosition.X < 0 ? blockPosition.X+WorldConfig.ChunkSize.X : blockPosition.X,
								blockPosition.Y < 0 ? blockPosition.Y+WorldConfig.ChunkSize.Y : blockPosition.Y,
								blockPosition.Z < 0 ? blockPosition.Z+WorldConfig.ChunkSize.Z : blockPosition.Z);
	return blockPosition;
}

FIntVector AWorldManager::GetBlockCoordinatesFromWorldLocation(const FVector Location) const
{
    const FVector calculatedLocation = (FVector(Location.X, Location.Y, Location.Z)+WorldConfig.GetChunkWorldSize()/2*FVector(1.0f, 1.0f, 0.0f)) / WorldConfig.BlockSize;
    const FIntVector blockLocation = FIntVector(floor(calculatedLocation.X), floor(calculatedLocation.Y), floor(calculatedLocation.Z));
	
	return blockLocation;
}

void AWorldManager::SetBlock(const FIntVector InPosition, const FBlock InBlock)
{
	const FIntVector chunkPosition = GetChunkPositionFromBlockWorldCoordinates(InPosition);
	const FIntVector tilePosition = GetBlockPositionFromWorldBlockCoordinates(InPosition);
	auto modifiedChunk = ModifiedChunks.Find(chunkPosition);
	if(modifiedChunk == nullptr)
	{
		modifiedChunk = &ModifiedChunks.Add({chunkPosition, FModifiedChunk()});
	}
	modifiedChunk->ModifiedBlocks.Add(tilePosition, InBlock);

	if(const auto chunk = Chunks.Find(chunkPosition); chunk != nullptr && *chunk != nullptr)
	{
		if(tilePosition.X == 0)
		{
			if(const auto neighbor = Chunks.Find(chunkPosition-FIntVector(1,0,0)); neighbor != nullptr && *chunk != nullptr)
			{
				(*neighbor)->AddBlock(FIntVector(WorldConfig.ChunkSize.X, tilePosition.Y, tilePosition.Z), InBlock);
				ChunkMeshesToGenerate.Enqueue(*neighbor);
			}
		}
		if(tilePosition.Y == 0)
		{
			if(const auto neighbor = Chunks.Find(chunkPosition-FIntVector(0,1,0)); neighbor != nullptr && *chunk != nullptr)
			{
				(*neighbor)->AddBlock(FIntVector(tilePosition.X, WorldConfig.ChunkSize.Y, tilePosition.Z), InBlock);
				ChunkMeshesToGenerate.Enqueue(*neighbor);
			}
		}
		if(tilePosition.Z == 0)
		{
			if(const auto neighbor = Chunks.Find(chunkPosition-FIntVector(0,0,1)); neighbor != nullptr && *chunk != nullptr)
			{
				(*neighbor)->AddBlock(FIntVector(tilePosition.X, tilePosition.Y, WorldConfig.ChunkSize.Z), InBlock);
				ChunkMeshesToGenerate.Enqueue(*neighbor);
			}
		}

		if(tilePosition.X == WorldConfig.ChunkSize.X-1)
		{
			if(const auto neighbor = Chunks.Find(chunkPosition+FIntVector(1,0,0)); neighbor != nullptr && *chunk != nullptr)
			{
				(*neighbor)->AddBlock(FIntVector(-1, tilePosition.Y, tilePosition.Z), InBlock);
				ChunkMeshesToGenerate.Enqueue(*neighbor);
			}
		}
		if(tilePosition.Y == WorldConfig.ChunkSize.Y-1)
		{
			if(const auto neighbor = Chunks.Find(chunkPosition+FIntVector(0,1,0)); neighbor != nullptr && *chunk != nullptr)
			{
				(*neighbor)->AddBlock(FIntVector(tilePosition.X, -1, tilePosition.Z), InBlock);
				ChunkMeshesToGenerate.Enqueue(*neighbor);
			}
		}
		if(tilePosition.Z == WorldConfig.ChunkSize.Z-1)
		{
			if(const auto neighbor = Chunks.Find(chunkPosition+FIntVector(0,0,1)); neighbor != nullptr && *chunk != nullptr)
			{
				(*neighbor)->AddBlock(FIntVector(tilePosition.X, tilePosition.Y, -1), InBlock);
				ChunkMeshesToGenerate.Enqueue(*neighbor);
			}
		}
		
		(*chunk)->AddBlock(tilePosition, InBlock);
		ChunkMeshesToGenerate.Enqueue(*chunk);
	}
}

void AWorldManager::RemoveBlock(const FIntVector InPosition)
{
	const FIntVector chunkPosition = GetChunkPositionFromBlockWorldCoordinates(InPosition);
	const FIntVector tilePosition = GetBlockPositionFromWorldBlockCoordinates(InPosition);
	
	UpdateChunk(chunkPosition, tilePosition);

	// Update Neighbors
	if( tilePosition.X == 0)
	{
		UpdateChunk(chunkPosition+FIntVector(-1,0,0), tilePosition+FIntVector(WorldConfig.ChunkSize.X, 0, 0));
	}
	if(tilePosition.Y == 0)
	{
		UpdateChunk(chunkPosition+FIntVector(0,-1,0), tilePosition+FIntVector(0, WorldConfig.ChunkSize.Y, 0));
	}
	if(tilePosition.Z == 0)
	{
		UpdateChunk(chunkPosition+FIntVector(0,0,-1), tilePosition+FIntVector(0, 0, WorldConfig.ChunkSize.Z));
	}
	if(tilePosition.X == WorldConfig.ChunkSize.X-1)
	{
		UpdateChunk(chunkPosition+FIntVector(1,0,0), tilePosition-FIntVector(WorldConfig.ChunkSize.X, 0, 0));
	}
	if(tilePosition.Y == WorldConfig.ChunkSize.Y-1)
	{
		UpdateChunk(chunkPosition+FIntVector(0,1,0), tilePosition-FIntVector(0, WorldConfig.ChunkSize.Y, 0));
	}
	if(tilePosition.Z == WorldConfig.ChunkSize.Z-1)
	{
		UpdateChunk(chunkPosition+FIntVector(0,0,1), tilePosition-FIntVector(0, 0, WorldConfig.ChunkSize.Z));
	}
}

bool AWorldManager::SaveWorld()
{
	bool result = true;
	for (const auto modifiedChunk : ModifiedChunks)
	{
		if(!ChunkStorage->SaveChunk(modifiedChunk.Key, modifiedChunk.Value.ModifiedBlocks))
		{
			result = false;
		}
	}
	return result;
}

void AWorldManager::ShowDebugLines(const bool Blocks, const bool Grid) const
{
	FlushPersistentDebugLines(GetWorld());
	for (const auto chunkMesh : ChunkMeshes)
	{
		chunkMesh.Value->ShowDebugLines(true, Blocks, Grid);
	}
}
