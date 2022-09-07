// Fill out your copyright notice in the Description page of Project Settings.


#include "World/SimpleGenerator.h"

#include "IStereoLayers.h"

void USimpleGenerator::Init()
{
	Noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	Noise.SetFractalType(FastNoiseLite::FractalType_FBm);
	Noise.SetSeed(NoiseConfig.Seed);
	Noise.SetFrequency(NoiseConfig.Frequency);
	Noise.SetFractalGain(NoiseConfig.Gain);
	Noise.SetFractalOctaves(NoiseConfig.Octaves);
	Noise.SetFractalLacunarity(NoiseConfig.Lacunarity);

	DistortionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	DistortionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
	DistortionNoise.SetSeed(DistortionNoiseConfig.Seed);
	DistortionNoise.SetFrequency(DistortionNoiseConfig.Frequency);
	DistortionNoise.SetFractalGain(DistortionNoiseConfig.Gain);
	DistortionNoise.SetFractalOctaves(DistortionNoiseConfig.Octaves);
	DistortionNoise.SetFractalLacunarity(DistortionNoiseConfig.Lacunarity);
	
	bHasBeenInitialized = true;
	auto SortPredicate = [&](const FBlockLayer& A, const FBlockLayer& B)->bool
	{
		return A.Height < B.Height;
	};
	Layers.Sort(SortPredicate);
}

float USimpleGenerator::GetNoise(const float X, const float Y)
{
	float height = Noise.GetNoise(X, Y)/2.0f + 0.5f;
	height = FMath::Pow(height, NoiseConfig.Power);
	return height;
}

TOptional<FBlock> USimpleGenerator::GetTile(const FIntVector& Position, const FWorldConfig& WorldConfig)
{
	if(!bHasBeenInitialized) Init();
	const int MaxHeight = WorldConfig.GetWorldBlockHeight();
	if(const int noiseHeight = round(GetNoise(static_cast<float>(Position.X),static_cast<float>(Position.Y)) * MaxHeight); noiseHeight >= Position.Z)
	{
		const float distortionNoiseHeight = (DistortionNoise.GetNoise(static_cast<float>(Position.X),static_cast<float>(Position.Y))/2.0f + 0.5f)*0.25;
		const float blockHeightPercentage = static_cast<float>(Position.Z)/MaxHeight;

		for (const auto layer : Layers)
		{
			const float layerHeight = layer.Height + distortionNoiseHeight;
			if(blockHeightPercentage < layerHeight)
			{
				if(layer.bTopBlockDiffers &&  Position.Z+1 > noiseHeight)
				{
					return TOptional(FBlock(layer.TopBlockTypeId));
				}
				return TOptional(FBlock(layer.BlockTypeId));
			}
		}
	}
	return TOptional<FBlock>();
}