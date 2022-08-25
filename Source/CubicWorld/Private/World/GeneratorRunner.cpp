// Fill out your copyright notice in the Description page of Project Settings.


#include "World/GeneratorRunner.h"
#include "Globals.h"


DECLARE_CYCLE_STAT(TEXT("generate chunks in generator runner"), STAT_Generator, STATGROUP_CubicWorld);

#pragma region Main Thread Code

FGeneratorRunner::FGeneratorRunner(UGenerator* InGenerator, const FWorldConfig& InWorldConfig) :
	Generator(InGenerator),
	WorldConfig(InWorldConfig)
{
	Thread = FRunnableThread::Create(this, TEXT("GeneratorThread"));
}
FGeneratorRunner::~FGeneratorRunner()
{
	if (Thread != nullptr)
	{
		Thread->Kill();
		delete Thread;
	}
}

#pragma endregion

bool FGeneratorRunner::Init()
{
	bShouldRun = true;
	return true;
}

uint32 FGeneratorRunner::Run()
{
	LLM_SCOPE(ELLMTag::Landscape);
	if(Generator == nullptr) return -1;
	while (bShouldRun)
	{
		if(!Tasks.IsEmpty())
		{
			SCOPE_CYCLE_COUNTER(STAT_Generator);
			SCOPED_NAMED_EVENT(FGeneratorRunner_Generate, FColor::Red);
			TPair<FIntVector, TMap<FIntVector, FTile>> task;
			Tasks.Dequeue(task);
			FChunkConfig chunkConfig = FChunkConfig(WorldConfig, task.Key);
			TMap<FIntVector, FTile> tiles = task.Value;
			Generator->GenerateChunk(chunkConfig, tiles);
			TPair<FIntVector, TMap<FIntVector, FTile>> result;
			result.Key = task.Key;
			result.Value = tiles;
			Results.Enqueue(result);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Generator runner stopped"))
	bHasStopped = true;
	return 0;
}

void FGeneratorRunner::Stop()
{
	Tasks.Empty();
	Results.Empty();
	bShouldRun = false;
	while(!bHasStopped)
	{
		
	}
}


