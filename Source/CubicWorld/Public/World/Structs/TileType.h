﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TileType.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FTileType
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString TileType = "";
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsSolid = true;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSideDiffers = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 TextureId = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 SideTextureId = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 BottomTextureId = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor Color = FColor::Magenta;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor SideColor = FColor::Magenta;
	
};
