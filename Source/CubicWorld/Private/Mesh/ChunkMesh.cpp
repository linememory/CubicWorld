// Fill out your copyright notice in the Description page of Project Settings.


#include "Mesh/ChunkMesh.h"

#include "Mesh/RuntimeMeshProviderChunk.h"
#include "Providers/RuntimeMeshProviderCollision.h"


bool AChunkMesh::GetHasCollision() const
{
	return bHasCollision;
}

void AChunkMesh::SetHasCollision(const bool bInHasCollision)
{
	bHasCollision = bInHasCollision;
	static_cast<URuntimeMeshProviderChunk*>(RuntimeMeshComponent->GetProvider())->SetHasCollision(bHasCollision);
	RuntimeMeshComponent->GetRuntimeMesh()->MarkAllLODsDirty();
}


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
			ChunkProvider->SetHasCollision(bHasCollision);
			// RMC->Initialize(ChunkProvider);
		
			URuntimeMeshProviderCollision* ChunkCollisionProvider = NewObject<URuntimeMeshProviderCollision>();
			ChunkCollisionProvider->SetChildProvider(ChunkProvider);
			ChunkCollisionProvider->SetRenderableLODForCollision(0);
			ChunkCollisionProvider->SetRenderableSectionAffectsCollision(0, true);
		
			FRuntimeMeshCollisionSettings CollisionSettings;
			CollisionSettings.bUseComplexAsSimple = true;
			CollisionSettings.bUseAsyncCooking = true;
			ChunkCollisionProvider->SetCollisionSettings(CollisionSettings);
			RMC->Initialize(ChunkCollisionProvider);

			
			// const FWorldConfig& WorldConfig = Chunk->ChunkConfig.WorldConfig;
			// DrawDebugBox(GetWorld(), GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5f), WorldConfig.GetChunkWorldSize()/2, FColor::Orange, true, -1, 0, 2);
			// DrawDebugSolidBox(GetWorld(), GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5f), WorldConfig.GetChunkWorldSize()/2*1.0001, FColor::Orange.WithAlpha(5), true);

			// for (int Z = 0; Z < WorldConfig.ChunkSize.Z; Z++)
			// {
			// 	for (int Y = 0; Y < WorldConfig.ChunkSize.Y; ++Y)
			// 	{
			// 		for (int X = 0; X < WorldConfig.ChunkSize.X; ++X)
			// 		{
			// 			FVector position = (FVector(X,Y,Z)+FVector(0.5,0.5,0.5))*WorldConfig.BlockSize + GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5) - WorldConfig.GetChunkWorldSize()/2;
			// 			DrawDebugBox(GetWorld(), position, WorldConfig.BlockSize/2*1.00014, FColor::Purple, true, -1, 0, 0.2);
			// 			// DrawDebugSolidBox(GetWorld(), position, WorldConfig.BlockSize/2*1.0001, FColor::Blue.WithAlpha(2), true);
			// 		}
			// 	}
			// }
			// for (auto tile : Chunk->GetTiles())
			// {
			// 	FVector position = (FVector(tile.Key)+FVector(0.5,0.5,0.5))*WorldConfig.BlockSize + GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5) - WorldConfig.GetChunkWorldSize()/2;
			// 	DrawDebugBox(GetWorld(), position, WorldConfig.BlockSize/2*1.00015, FColor::Blue, true, -1, 0, 0.5);
			// 	// DrawDebugSolidBox(GetWorld(), position, WorldConfig.BlockSize/2*1.0001, FColor::Blue.WithAlpha(2), true);
			// }


			// for (auto tile : Chunk->GetTiles())
			// {
			// 	const int multiplier = 2;
			// 	if(	tile.Key.X % multiplier == 0 &&
			// 		tile.Key.Y % multiplier == 0 &&
			// 		tile.Key.Z % multiplier == 0 ) 
			// 	{
			// 	
			// 		FVector position = (FVector(tile.Key)+FVector(1,1,1))*WorldConfig.BlockSize + GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5) - WorldConfig.GetChunkWorldSize()/2;
			// 		DrawDebugBox(GetWorld(), position, WorldConfig.BlockSize*1.00025, FColor::Green, true, -1, 0, 1.5);
			// 		// DrawDebugSolidBox(GetWorld(), position, WorldConfig.BlockSize*1.0002, FColor::Green.WithAlpha(2), true);
			// 	}
			// }
		}
	}
	
}
