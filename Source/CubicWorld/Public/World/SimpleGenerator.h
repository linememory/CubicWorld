// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FastNoiseLite.h"
#include "Generator.h"
#include "SimpleGenerator.generated.h"

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

private:
	FastNoiseLite Noise;
	bool bHasBeenInitialized = false;
	void Init();

	float GetNoise(float X, float Y);

public:
	virtual TOptional<FTile> GetTile(const FIntVector &Position, const FWorldConfig &WorldConfig) override;
};
