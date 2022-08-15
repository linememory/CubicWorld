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
	bRunThread = true;
	return true;
}

uint32 FGeneratorRunner::Run()
{
	LLM_SCOPE(ELLMTag::Landscape);
	if(Generator == nullptr) return -1;
	while (bRunThread)
	{
		if(!Tasks.IsEmpty())
		{
			SCOPE_CYCLE_COUNTER(STAT_Generator);
			SCOPED_NAMED_EVENT(FGeneratorRunner_Generate, FColor::Red);
			TPair<FIntVector, TMap<FIntVector, FTile>> task;
			Tasks.Dequeue(task);
			FChunkConfig chunkConfig = FChunkConfig(WorldConfig, task.Key);
			TMap<FIntVector, FTile> tiles = Generator->GenerateChunk(chunkConfig, task.Value);
			TPair<FIntVector, TMap<FIntVector, FTile>> result;
			result.Key = task.Key;
			result.Value = tiles;
			Results.Enqueue(result);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Generator runner stopped"))
	return 0;
}

void FGeneratorRunner::Stop()
{
	bRunThread = false;
}


