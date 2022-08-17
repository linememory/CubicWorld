// Fill out your copyright notice in the Description page of Project Settings.


#include "World/SimpleGenerator.h"

void USimpleGenerator::Init()
{
	Noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	Noise.SetFractalType(FastNoiseLite::FractalType_FBm);
	Noise.SetSeed(NoiseConfig.Seed);
	Noise.SetFrequency(NoiseConfig.Frequency);
	Noise.SetFractalGain(NoiseConfig.Gain);
	Noise.SetFractalOctaves(NoiseConfig.Octaves);
	Noise.SetFractalLacunarity(NoiseConfig.Lacunarity);
	bHasBeenInitialized = true;
}

float USimpleGenerator::GetNoise(const float X, const float Y)
{
	float height = Noise.GetNoise(X, Y)/2.0f + 0.5f;
	height = FMath::Pow(height, NoiseConfig.Power);
	return height;
}

TOptional<FTile> USimpleGenerator::GetTile(const FIntVector& Position, FWorldConfig& WorldConfig)
{
	if(!bHasBeenInitialized) Init();
	const int MaxHeight = WorldConfig.GetWorldBlockHeight();
	if(const int noiseHeight = round(GetNoise(static_cast<float>(Position.X),static_cast<float>(Position.Y)) * MaxHeight); noiseHeight >= Position.Z)
	{
		const float tileHeightPercentage = static_cast<float>(Position.Z)/MaxHeight;
		int index = 0;
		if(tileHeightPercentage < 0.1)
			index = 3;
		else if(tileHeightPercentage < 0.6)
		{
			if(Position.Z+1 > noiseHeight)
				index = 1;
			else
				index = 0;
		}
		else if(tileHeightPercentage < 0.8)
			index = 2;
		else if(tileHeightPercentage <= 1)
			index = 4;
		return TOptional<FTile>(FTile(index));
	}
	return TOptional<FTile>();
}