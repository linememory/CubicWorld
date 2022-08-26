// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshComponent.h"
#include "RuntimeMeshProviderChunk.h"
#include "GameFramework/Actor.h"
#include "World/Chunk.h"
#include "ChunkMesh.generated.h"

UCLASS(BlueprintType, Blueprintable)
class CUBICWORLD_API AChunkMesh final : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CubicWorld|Chunk")
	const UChunk *Chunk;

private:
	UPROPERTY(EditAnywhere)
	URuntimeMeshComponent *RuntimeMeshComponent;
	UPROPERTY()
	URuntimeMeshProviderChunk *ChunkProvider;

public:
	// Sets default values for this actor's properties
	AChunkMesh();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable)
	void GenerateMesh();

	UFUNCTION(BlueprintCallable)
	void ShowDebugLines(bool ChunkDebugLines = false, bool BlocksDebugLines = false, bool GridDebugLines = false) const;
};
