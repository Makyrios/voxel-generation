// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkWorld.generated.h"

class AChunk;
class AVoxelGenerationCharacter;

UCLASS()
class VOXELGEN_API AChunkWorld : public AActor
{
	GENERATED_BODY()
	
public:	
	AChunkWorld();
	
	void InitializeWorld();

	const TMap<FIntVector2, AChunk*>& GetChunksData() { return ChunksData; }

protected:
	virtual void BeginPlay() override;

private:
	void UpdateChunks();

	void ProcessChunksGeneration(const float DeltaTime);

	bool IsPlayerChunkUpdated();

	AChunk* LoadChunkAtPosition(const FIntVector2& ChunkCoordinates);

public:
	virtual void Tick(float DeltaTime) override;

private:
	TMap<FIntVector2, AChunk*> ChunksData;
	TQueue<AChunk*> ChunkGenerationQueue;
	
	UPROPERTY(EditAnywhere, Category = "Chunk World")
	float SpawnChunkDelay = 0.02f;
	float CurrentSpawnChunkDelay = SpawnChunkDelay;
	
	FIntVector2 CurrentPlayerChunk;
	
	UPROPERTY()
	AVoxelGenerationCharacter* PlayerCharacter;
	
	UPROPERTY(EditDefaultsOnly, Category = "Chunk World")
	TSubclassOf<AChunk> ChunkClass;

	UPROPERTY(EditAnywhere, Category = "Chunk World")
	int DrawDistance = 5;
	
};
