// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChunkWorld.h"
#include "Actors/Chunk.h"
#include "Objects/ChunkData.h"

AChunkWorld::AChunkWorld()
{
	PrimaryActorTick.bCanEverTick = false;

}

void AChunkWorld::BeginPlay()
{
	Super::BeginPlay();

	CreateChunks();
}

void AChunkWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AChunkWorld::CreateChunks()
{
	for (int x = 0; x < FChunkData::WorldSizeInChunks; ++x)
	{
		for (int y = 0; y < FChunkData::WorldSizeInChunks; y++)
		{
			float xPos = x * FChunkData::ChunkSize * FChunkData::BlockSize * FChunkData::BlockScale;
			float yPos = y * FChunkData::ChunkSize * FChunkData::BlockSize * FChunkData::BlockScale;
			AChunk* Chunk = GetWorld()->SpawnActor<AChunk>(ChunkClass, FVector(xPos, yPos, 0), FRotator::ZeroRotator);
			ChunksData.Add(FVector2D(x, y), Chunk);
		}
	}
}

