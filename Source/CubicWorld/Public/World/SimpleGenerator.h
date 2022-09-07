// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FastNoiseLite.h"
#include "Generator.h"
#include "SimpleGenerator.generated.h"

USTRUCT(BlueprintType)
struct FBlockLayer
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(UIMin = 0.0, UIMax = 1.0))
	float Height = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 BlockTypeId = UINT8_MAX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bTopBlockDiffers = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 TopBlockTypeId = UINT8_MAX;
	
};

USTRUCT(BlueprintType)
struct FNoiseConfig
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Seed = 1337;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Frequency = 0.01f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Octaves = 3;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Gain = 0.5f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Lacunarity = 2.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Power = 1.0f;
};


/**
 *
 */
UCLASS(BlueprintType, Blueprintable)
class CUBICWORLD_API USimpleGenerator final : public UGenerator
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FNoiseConfig NoiseConfig;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FNoiseConfig DistortionNoiseConfig;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FBlockLayer> Layers;

private:
	FastNoiseLite Noise;
	FastNoiseLite DistortionNoise;
	bool bHasBeenInitialized = false;
	void Init();

	float GetNoise(float X, float Y);

public:
	virtual TOptional<FBlock> GetTile(const FIntVector &Position, const FWorldConfig &WorldConfig) override;
};
