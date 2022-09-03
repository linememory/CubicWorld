// Fill out your copyright notice in the Description page of Project Settings.

#include "World/WorldManager.h"

#include <algorithm>
#include <concrt.h>

#include "Globals.h"
#include "IContentBrowserSingleton.h"
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
				for (int Y = -distance-1; Y <= distance+1; ++Y)
				{
					for (int X = -distance-1; X <= distance+1; ++X)
					{
						FIntVector ChunkToLoad{position.X+X, position.Y+Y, Z};
						const int32 index = ChunksToLoad.Find(ChunkToLoad); 
						if(index == INDEX_NONE && Chunks.Find(ChunkToLoad) == nullptr)
						{
							ChunksToLoad.Add(ChunkToLoad);
							ChunksToUnload.Remove(ChunkToLoad);
							if(abs(Y) <= distance && abs(X) <= distance)
                            {
                            	VisibleChunks.Add(ChunkToLoad);
                            }
						}else if(abs(Y) <= distance && abs(X) <= distance && VisibleChunks.Find(ChunkToLoad) == nullptr)
						{
							VisibleChunks.Add(ChunkToLoad);
							ChunkMeshesToGenerate.Enqueue(ChunkToLoad);
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
			{
				ChunksToUnload.Add(chunk.Key);
			}
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
			chunk->SetChunkConfig(chunkConfig);
			chunk->WorldChunks = &Chunks;
			Chunks.Add(chunkPosition, chunk);

			if(auto tiles = ChunkStorage->LoadChunk(chunkPosition); tiles.IsSet())
			{
				GeneratorRunner->Results.Enqueue({chunkPosition, tiles.GetValue()});
			} else
			{
				GeneratorRunner->Tasks.Enqueue({chunkPosition, TChunkData(chunkConfig.WorldConfig.ChunkSize)});
			}
		}
	}

	{
		TPair<FIntVector, TChunkData> tiles;
		SCOPED_NAMED_EVENT(AWorldManager_GetGeneratedBlocks, FColor::Blue);
		while (GeneratorRunner->Results.Dequeue(tiles))
		{
			if(UChunk** chunk = Chunks.Find(tiles.Key); chunk != nullptr && *chunk != nullptr)
			{
				(*chunk)->SetBlocks(tiles.Value);
				if(VisibleChunks.Find(tiles.Key) != nullptr)
					ChunkMeshesToGenerate.Enqueue(tiles.Key);
			}
		}
	}
	ChunksToLoad.Reset();
}

void AWorldManager::GenerateChunkMeshes()
{
	SCOPE_CYCLE_COUNTER(STAT_GenerateChunkMeshes);
	SCOPED_NAMED_EVENT(AWorldManager_GenerateChunkMeshes, FColor::Blue);
	TArray<FIntVector> Deferred;
	while (!ChunkMeshesToGenerate.IsEmpty())
	{
		FIntVector Position;
		ChunkMeshesToGenerate.Dequeue(Position);
		const UChunk* const * Chunk = Chunks.Find(Position);
		if(Chunk == nullptr || *Chunk == nullptr || !VisibleChunks.Find(Position))
		{
			continue;
		}
		const auto Top = Chunks.Find(Position+FIntVector(0,0,1));
		const auto Bottom = Chunks.Find(Position+FIntVector(0,0,-1));
		const auto Front = Chunks.Find(Position+FIntVector(0,1,0));
		const auto Back = Chunks.Find(Position+FIntVector(0,-1,0));
		const auto Right = Chunks.Find(Position+FIntVector(1,0,0));
		const auto Left = Chunks.Find(Position+FIntVector(-1,0,0));

		if(!(*Chunk)->bIsReady ||
			( Position.Z < WorldConfig.MaxChunksZ-1 && (Top == nullptr || *Top == nullptr || !(*Top)->bIsReady)) ||
			( Position.Z > 0 && Bottom == nullptr && (Bottom == nullptr || *Bottom == nullptr || !(*Bottom)->bIsReady)) ||
			Front == nullptr || Front == nullptr || *Front == nullptr || !(*Front)->bIsReady||
			Back == nullptr || Back == nullptr || *Back == nullptr || !(*Back)->bIsReady||
			Right == nullptr || Right == nullptr || *Right == nullptr || !(*Right)->bIsReady||
			Left == nullptr || Left == nullptr || *Left == nullptr || !(*Left)->bIsReady)
		{
			Deferred.Add(Position);
			continue;
		}
		AChunkMesh* ChunkMesh;
		if(const auto foundChunkMesh = ChunkMeshes.Find(Position); foundChunkMesh != nullptr && *foundChunkMesh != nullptr)
		{
			ChunkMesh = *foundChunkMesh;
			
		} else
		{
			FVector SpawnLocation = FVector(FVector(Position) * WorldConfig.GetChunkWorldSize());
			ChunkMesh =  GetWorld()->SpawnActor<AChunkMesh>(SpawnLocation, FRotator(0));
			ChunkMesh->Chunk = *Chunk;
			
			ChunkMeshes.Add(Position, ChunkMesh);
		}
		ChunkMesh->GenerateMesh();
	}
	for (auto chunkPosition : Deferred)
	{
		ChunkMeshesToGenerate.Enqueue(chunkPosition);
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
		}
		if(ModifiedChunks.Find(position) == INDEX_NONE)
			Chunks.Remove(position);
		VisibleChunks.Remove(position);
		ChunkMeshes.Remove(position);
	}
	ChunksToUnload.Reset();
}


TArray<FBlockType> AWorldManager::GetBlockTypes() const
{
	return WorldConfig.BlockTypes;
}

void AWorldManager::AddActorToTrack(AActor* InActorToTrack)
{
	UTrackable* trackable = NewObject<UTrackable>(InActorToTrack);
	trackable->RegisterComponent();
	UE_LOG(LogTemp, Warning, TEXT("Owner of trackable: %s"), *trackable->GetOwner()->GetName())
	TrackerComponents.Add(trackable);
}

FIntVector AWorldManager::GetChunkPositionFromBlockWorldCoordinates(const FIntVector& BlockPosition) const
{
	return FIntVector(floor(static_cast<float>(BlockPosition.X)/WorldConfig.ChunkSize.X), floor(static_cast<float>(BlockPosition.Y)/WorldConfig.ChunkSize.Y), floor(static_cast<float>(BlockPosition.Z)/WorldConfig.ChunkSize.Z));
}

FIntVector AWorldManager::GetBlockPositionFromWorldBlockCoordinates(const FIntVector& BlockPosition) const
{
	FIntVector blockPosition =  FIntVector(BlockPosition.X%WorldConfig.ChunkSize.X,BlockPosition.Y%WorldConfig.ChunkSize.Y,BlockPosition.Z%WorldConfig.ChunkSize.Z);
	blockPosition = FIntVector(	blockPosition.X < 0 ? blockPosition.X+WorldConfig.ChunkSize.X : blockPosition.X,
								blockPosition.Y < 0 ? blockPosition.Y+WorldConfig.ChunkSize.Y : blockPosition.Y,
								blockPosition.Z < 0 ? blockPosition.Z+WorldConfig.ChunkSize.Z : blockPosition.Z);
	return blockPosition;
}

FIntVector AWorldManager::GetBlockCoordinatesFromWorldLocation(const FVector& Location) const
{
    const FVector calculatedLocation = (FVector(Location.X, Location.Y, Location.Z)+WorldConfig.GetChunkWorldSize()/2*FVector(1.0f, 1.0f, 0.0f)) / WorldConfig.BlockSize;
    const FIntVector blockLocation = FIntVector(floor(calculatedLocation.X), floor(calculatedLocation.Y), floor(calculatedLocation.Z));
	
	return blockLocation;
}

FBlock AWorldManager::GetBlock(const FIntVector& InPosition) const
{
	const FIntVector chunkPosition = GetChunkPositionFromBlockWorldCoordinates(InPosition);
	const FIntVector tilePosition = GetBlockPositionFromWorldBlockCoordinates(InPosition);

	if(const auto chunk = Chunks.Find(chunkPosition); chunk != nullptr && *chunk != nullptr)
	{
		if(const auto block = (*chunk)->GetBlock(tilePosition); block != Air)
		{
			return block;
		}
	}
	return FBlock();
}

void AWorldManager::SetBlock(const FIntVector& InPosition, const FBlock& InBlock)
{
	const FIntVector chunkPosition = GetChunkPositionFromBlockWorldCoordinates(InPosition);
	const FIntVector tilePosition = GetBlockPositionFromWorldBlockCoordinates(InPosition);
	if(ModifiedChunks.Find(chunkPosition) == INDEX_NONE)
	{
		ModifiedChunks.Add(chunkPosition);
	}
	
	if(const auto chunk = Chunks.Find(chunkPosition); chunk != nullptr && *chunk != nullptr)
	{
		(*chunk)->AddBlock(tilePosition, InBlock);
		ChunkMeshesToGenerate.Enqueue(chunkPosition);
		if(tilePosition.X == 0)
		{
			const FIntVector Position = chunkPosition-FIntVector(1,0,0);
			if(const auto neighbor = Chunks.Find(chunkPosition-FIntVector(1,0,0)); neighbor != nullptr && *neighbor != nullptr)
			{
				ChunkMeshesToGenerate.Enqueue(Position);
			}
		}
		if(tilePosition.Y == 0)
		{
			const FIntVector Position = chunkPosition-FIntVector(0,1,0);
			if(const auto neighbor = Chunks.Find(chunkPosition-FIntVector(0,1,0)); neighbor != nullptr && *neighbor != nullptr)
			{
				ChunkMeshesToGenerate.Enqueue(Position);
			}
		}
		if(tilePosition.Z == 0)
		{
			const FIntVector Position = chunkPosition-FIntVector(0,0,1);
			if(const auto neighbor = Chunks.Find(chunkPosition-FIntVector(0,0,1)); neighbor != nullptr && *neighbor != nullptr)
			{
				ChunkMeshesToGenerate.Enqueue(Position);
			}
		}

		if(tilePosition.X == WorldConfig.ChunkSize.X-1)
		{
			const FIntVector Position = chunkPosition+FIntVector(1,0,0);
			if(const auto neighbor = Chunks.Find(chunkPosition+FIntVector(1,0,0)); neighbor != nullptr && *neighbor != nullptr)
			{
				ChunkMeshesToGenerate.Enqueue(Position);
			}
		}
		if(tilePosition.Y == WorldConfig.ChunkSize.Y-1)
		{
			const FIntVector Position = chunkPosition+FIntVector(0,1,0);
			if(const auto neighbor = Chunks.Find(chunkPosition+FIntVector(0,1,0)); neighbor != nullptr && *neighbor != nullptr)
			{
				ChunkMeshesToGenerate.Enqueue(Position);
			}
		}
		if(tilePosition.Z == WorldConfig.ChunkSize.Z-1)
		{
			const FIntVector Position = chunkPosition+FIntVector(0,0,1) ;
			if(const auto neighbor = Chunks.Find(chunkPosition+FIntVector(0,0,1)); neighbor != nullptr && *neighbor != nullptr)
			{
				ChunkMeshesToGenerate.Enqueue(Position);
			}
		}
	}
}

FBlock AWorldManager::RemoveBlock(const FIntVector& InPosition)
{
	const FIntVector chunkPosition = GetChunkPositionFromBlockWorldCoordinates(InPosition);
	const FIntVector tilePosition = GetBlockPositionFromWorldBlockCoordinates(InPosition);

	const FBlock block = GetBlock(InPosition);
	if(block != Air)
	{
		RemoveBlock(chunkPosition, tilePosition);

		// Update Neighbors
		if( tilePosition.X == 0)
		{
			ChunkMeshesToGenerate.Enqueue(chunkPosition+FIntVector(-1,0,0));
		}
		if(tilePosition.Y == 0)
		{
			ChunkMeshesToGenerate.Enqueue(chunkPosition+FIntVector(0,-1,0));
		}
		if(tilePosition.Z == 0)
		{
			ChunkMeshesToGenerate.Enqueue(chunkPosition+FIntVector(0,0,-1));
		}
		if(tilePosition.X == WorldConfig.ChunkSize.X-1)
		{
			ChunkMeshesToGenerate.Enqueue(chunkPosition+FIntVector(1,0,0));
		}
		if(tilePosition.Y == WorldConfig.ChunkSize.Y-1)
		{
			ChunkMeshesToGenerate.Enqueue(chunkPosition+FIntVector(0,1,0));
		}
		if(tilePosition.Z == WorldConfig.ChunkSize.Z-1)
		{
			ChunkMeshesToGenerate.Enqueue(chunkPosition+FIntVector(0,0,1));
		}
	}
	return block;
}

void AWorldManager::RemoveBlock(const FIntVector& InChunkPosition, const FIntVector& InBlockPosition)
{
	if(const auto chunk = Chunks.Find(InChunkPosition); chunk != nullptr && *chunk != nullptr)
	{
		(*chunk)->RemoveBlock(InBlockPosition);
		ChunkMeshesToGenerate.Enqueue(InChunkPosition);

		if(ModifiedChunks.Find(InChunkPosition) == INDEX_NONE)
		{
			ModifiedChunks.Add(InChunkPosition);
		}
	}
}

bool AWorldManager::SaveWorld()
{
	bool result = true;
	for (const FIntVector& modifiedChunkPosition : ModifiedChunks)
	{
		if(const auto modifiedChunk = Chunks.Find(modifiedChunkPosition); modifiedChunk != nullptr && *modifiedChunk != nullptr)
		{
			if(!ChunkStorage->SaveChunk(modifiedChunkPosition, (*modifiedChunk)->GetBlocks()))
			{
				result = false;
			}
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
