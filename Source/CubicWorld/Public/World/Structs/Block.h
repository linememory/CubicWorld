// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Block.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FBlock
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
    uint8 BlockTypeID = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsVisible = true;

	FBlock(): BlockTypeID(UINT8_MAX), bIsVisible(false){};
	explicit FBlock(const uint8 InTileID):BlockTypeID(InTileID){}

	friend FArchive& operator<<( FArchive& Ar, FBlock& Block )
	{
		return Ar << Block.BlockTypeID << Block.bIsVisible;
	}

	bool operator==(const FBlock& rhs) const
	{
		return BlockTypeID == rhs.BlockTypeID && bIsVisible == rhs.bIsVisible;
	}

	bool operator!=(const FBlock& rhs) const
	{
		return !(*this == rhs);
	}
};

static const FBlock Air = FBlock();
