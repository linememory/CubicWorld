﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generator.h"
#include "Structs/Block.h"

/**
 *
 */

class CUBICWORLD_API FGeneratorRunner final : public FRunnable
{
public:
	TQueue<TPair<FIntVector, TMap<FIntVector, FBlock>>> Tasks;
	TQueue<TPair<FIntVector, TMap<FIntVector, FBlock>>> Results;

	FGeneratorRunner(UGenerator *InGenerator, const FWorldConfig &InWorldConfig);
	virtual uint32 Run() override;
	virtual bool Init() override;
	virtual void Stop() override;
	virtual ~FGeneratorRunner() override;

private:
	FRunnableThread *Thread;
	bool bShouldRun = true;
	bool bHasStopped = false;

	UGenerator *const Generator;
	FWorldConfig WorldConfig;
};