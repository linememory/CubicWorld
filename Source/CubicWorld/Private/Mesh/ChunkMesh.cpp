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
			// DrawDebugBox(GetWorld(), GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5f), WorldConfig.GetChunkWorldSize()/2, FColor::Orange, true);
			// DrawDebugSolidBox(GetWorld(), GetActorLocation()+WorldConfig.GetChunkWorldSize()*FVector(0,0,0.5f), WorldConfig.GetChunkWorldSize()/2*1.001, FColor::Blue.WithAlpha(5), true);
		}
	}
	
}
