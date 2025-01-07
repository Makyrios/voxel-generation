// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChunkWorld.h"
#include "Actors/Chunk.h"
#include "Kismet/GameplayStatics.h"
#include "Structs/ChunkData.h"
#include "Player/Character/VoxelGenerationCharacter.h"

AChunkWorld::AChunkWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	
}

void AChunkWorld::BeginPlay()
{
	Super::BeginPlay();

	InitializeWorld();
}

void AChunkWorld::InitializeWorld()
{
	PlayerCharacter = Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// Wait until first chunks are loaded
	SetTickableWhenPaused(true);
	UGameplayStatics::SetGamePaused(GetWorld(), true);
}

void AChunkWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	UpdateChunks();
	ProcessChunksGeneration(DeltaTime);
}

void AChunkWorld::UpdateChunks()
{
	// Update only when player moved to another chunk
	if (!IsPlayerChunkUpdated() && !UGameplayStatics::IsGamePaused(GetWorld())) return;
	
	int LoadDistance = DrawDistance + 1;
	// Create chunks +1 which should be visible to player to remove extra polygons between chunks
	for (int x = CurrentPlayerChunk.X - LoadDistance; x <= CurrentPlayerChunk.X + LoadDistance; ++x)
	{
		for (int y = CurrentPlayerChunk.Y - LoadDistance; y <= CurrentPlayerChunk.Y + LoadDistance; ++y)
		{
			if(ChunksData.Contains(FIntVector2(x, y))) continue;

			LoadChunkAtPosition(FIntVector2(x, y));
		}
	}
	// Generate mesh for chunks which should be visible
	for (int x = CurrentPlayerChunk.X - DrawDistance; x <= CurrentPlayerChunk.X + DrawDistance; ++x)
	{
		for (int y = CurrentPlayerChunk.Y - DrawDistance; y <= CurrentPlayerChunk.Y + DrawDistance; ++y)
		{
			AChunk* LoadedChunk = ChunksData[FIntVector2(x, y)];
			if (LoadedChunk && !LoadedChunk->IsMeshInitialized() && !LoadedChunk->bIsProcessingMesh)
			{
				LoadedChunk->bIsProcessingMesh = true;
				ChunkGenerationQueue.Enqueue(LoadedChunk);
				UE_LOG(LogTemp, Warning, TEXT("Enqueuing Chunk at Position: %s"), *LoadedChunk->ChunkPosition.ToString());
			}
		}
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
	Chunk->GenerateBlocks(ChunkWorldPosition);
	Chunk->SetParentWorld(this);
	ChunksData.Add(ChunkCoordinates, Chunk);
	
	return Chunk;
}

void AChunkWorld::ProcessChunksGeneration(const float DeltaTime)
{
	CurrentSpawnChunkDelay -= DeltaTime;

	if (CurrentSpawnChunkDelay <= 0.f && !ChunkGenerationQueue.IsEmpty())
	{
		AChunk* Chunk = nullptr;
		if (ChunkGenerationQueue.Dequeue(Chunk))
		{
			UE_LOG(LogTemp, Warning, TEXT("Dequeuing Chunk at Position: %s"), *Chunk->ChunkPosition.ToString());
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