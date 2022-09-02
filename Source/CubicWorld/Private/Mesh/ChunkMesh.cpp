// Fill out your copyright notice in the Description page of Project Settings.


#include "Mesh/ChunkMesh.h"

#include "Mesh/RuntimeMeshProviderChunk.h"
#include "Providers/RuntimeMeshProviderCollision.h"


AChunkMesh::AChunkMesh()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}


void AChunkMesh::BeginPlay()
{
	Super::BeginPlay();
	
}


void AChunkMesh::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

void AChunkMesh::BeginDestroy()
{
	if(ChunkProvider != nullptr)
	{
		ChunkProvider->bMarkedForDestroy = true;
	}
	Super::BeginDestroy();
}

void AChunkMesh::GenerateMesh()
{
	if(RuntimeMeshComponent != nullptr)
	{
		RuntimeMeshComponent->GetRuntimeMesh()->MarkAllLODsDirty();
	} else
	{
		URuntimeMeshComponent* RMC = NewObject<URuntimeMeshComponent>(this);
		RMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		RMC->RegisterComponent();
		RMC->SetRelativeTransform(FTransform(FVector(0,0,0)));
		RMC->SetRuntimeMeshMobility(ERuntimeMeshMobility::Stationary);
		RuntimeMeshComponent = RMC;

		if(ChunkProvider == nullptr)
		{
			ChunkProvider = NewObject<URuntimeMeshProviderChunk>();
		}

		if(ChunkProvider != nullptr)
		{
			ChunkProvider->SetChunk(Chunk);
	
			URuntimeMeshProviderCollision* ChunkCollisionProvider = NewObject<URuntimeMeshProviderCollision>();
			ChunkCollisionProvider->SetChildProvider(ChunkProvider);
			ChunkCollisionProvider->SetRenderableLODForCollision(0);
			ChunkCollisionProvider->SetRenderableSectionAffectsCollision(0, true);
		
			FRuntimeMeshCollisionSettings CollisionSettings;
			CollisionSettings.bUseComplexAsSimple = true;
			CollisionSettings.bUseAsyncCooking = true;
			ChunkCollisionProvider->SetCollisionSettings(CollisionSettings);
			RMC->Initialize(ChunkCollisionProvider);
		}
	}
	
}

void AChunkMesh::ShowDebugLines(bool ChunkDebugLines, bool BlocksDebugLines, bool GridDebugLines) const
{
	const FWorldConfig& WorldConfig = Chunk->GetChunkConfig().WorldConfig;
	if(ChunkDebugLines)
	{
		DrawDebugBox(GetWorld(), GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5f), WorldConfig.GetChunkWorldSize()/2, FColor::Orange, true, -1, 0, 2);
	}
	if(BlocksDebugLines)
	{
		for (auto tile : Chunk->GetBlocks())
		{
			FVector position = (FVector(tile.Key)+FVector(0.5,0.5,0.5))*WorldConfig.BlockSize + GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5) - WorldConfig.GetChunkWorldSize()/2;
			DrawDebugBox(GetWorld(), position, WorldConfig.BlockSize/2*1.00015, FColor::Blue, true, -1, 0, 0.5);
		}
	}
	if(GridDebugLines)
	{
		for (int Z = 0; Z < WorldConfig.ChunkSize.Z; Z++)
		{
			for (int Y = 0; Y < WorldConfig.ChunkSize.Y; ++Y)
			{
				for (int X = 0; X < WorldConfig.ChunkSize.X; ++X)
				{
					FVector position = (FVector(X,Y,Z)+FVector(0.5,0.5,0.5))*WorldConfig.BlockSize + GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5) - WorldConfig.GetChunkWorldSize()/2;
					if(!(BlocksDebugLines && Chunk->GetBlocks().Find({X,Y,Z}) != nullptr))
					{
						DrawDebugBox(GetWorld(), position, WorldConfig.BlockSize/2*1.00014, FColor::Purple, true, -1, 0, 0.2);
					}
				}
			}
		}
	}

	
}
