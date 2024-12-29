// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChunkWorld.h"
#include "Actors/Chunk.h"
#include "Kismet/GameplayStatics.h"
#include "Objects/ChunkData.h"
#include "Player/Character/VoxelGenerationCharacter.h"

AChunkWorld::AChunkWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	
}

void AChunkWorld::InitializeWorld()
{
	PlayerCharacter = Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// Wait until first chunks are loaded
	SetTickableWhenPaused(true);
	UGameplayStatics::SetGamePaused(GetWorld(), true);
}

void AChunkWorld::BeginPlay()
{
	Super::BeginPlay();

	InitializeWorld();
}

void AChunkWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	UpdateChunks();
	ProcessChunksGeneration(DeltaTime);
}

void AChunkWorld::UpdateChunks()
{
	if (!IsPlayerChunkUpdated()) return;

	for (int x = CurrentPlayerChunk.X - DrawDistance; x < CurrentPlayerChunk.X + DrawDistance; ++x)
	{
		for (int y = CurrentPlayerChunk.Y - DrawDistance; y < CurrentPlayerChunk.Y + DrawDistance; ++y)
		{
			if(ChunksData.Contains(FIntVector2(x, y))) continue;

			AChunk* LoadedChunk = LoadChunkAtPosition(FIntVector2(x, y));
			ChunkGenerationQueue.Enqueue(LoadedChunk);
		}
	}
}

void AChunkWorld::ProcessChunksGeneration(const float DeltaTime)
{
	CurrentSpawnChunkDelay -= DeltaTime;

	if (CurrentSpawnChunkDelay <= 0.f && !ChunkGenerationQueue.IsEmpty())
	{
		AChunk* Chunk = nullptr;
		if (ChunkGenerationQueue.Dequeue(Chunk))
		{
			Chunk->RegenerateMeshAsync();
		}
		CurrentSpawnChunkDelay = SpawnChunkDelay;
	}

	// Unpause the game when all chunks are loaded
	if (UGameplayStatics::IsGamePaused(GetWorld()) && ChunkGenerationQueue.IsEmpty())
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}
}

bool AChunkWorld::IsPlayerChunkUpdated()
{
	PlayerCharacter = PlayerCharacter ? PlayerCharacter : Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!PlayerCharacter) return false;
	
	const FIntVector2 PlayerChunk = FChunkData::GetChunkPosition(PlayerCharacter->GetActorLocation());
	if (PlayerChunk != CurrentPlayerChunk)
	{
		CurrentPlayerChunk = PlayerChunk;
		return true;
	}
	return false;
}

AChunk* AChunkWorld::LoadChunkAtPosition(const FIntVector2& ChunkCoordinates)
{
	float xPos = ChunkCoordinates.X * FChunkData::ChunkSize * FChunkData::BlockScaledSize;
	float yPos = ChunkCoordinates.Y * FChunkData::ChunkSize * FChunkData::BlockScaledSize;
	FVector ChunkWorldPosition(xPos, yPos, 0);
    
	AChunk* Chunk = GetWorld()->SpawnActor<AChunk>(ChunkClass, ChunkWorldPosition, FRotator::ZeroRotator);
	Chunk->ChunkPosition = ChunkCoordinates;
	Chunk->SetParentWorld(this);
	ChunksData.Add(ChunkCoordinates, Chunk);

	// Generate blocks only; defer mesh creation to async task
	Chunk->InitializeChunk();
	
	return Chunk;
}