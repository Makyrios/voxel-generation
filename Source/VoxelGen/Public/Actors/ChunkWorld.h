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
	virtual void Tick(float DeltaTime) override;

private:
	void UpdateChunks();
	void ProcessChunksMeshGeneration(float DeltaTime);
	void ProcessChunksMeshClear(float DeltaTime);

	bool IsPlayerChunkUpdated();
	AChunk* LoadChunkAtPosition(const FIntVector2& ChunkCoordinates);

	void ActivateVisibleChunks(const FIntVector2& ChunkCoordinates);
	bool IsInsideDrawDistance(const FIntVector2& ChunkCoordinates, int x, int y);
	void DeactivatePreviousChunks(const TArray<FIntVector2>& PreviousVisibleChunks);

	void EnqueueChunkForGeneration(AChunk* Chunk);
	void EnqueueChunkForClearing(AChunk* Chunk);

	void PauseGameIfChunksLoadingComplete() const;

private:
	TMap<FIntVector2, AChunk*> ChunksData;
	TQueue<AChunk*> ChunkGenerationQueue;
	TQueue<AChunk*> ChunkClearQueue;

	UPROPERTY(EditAnywhere, Category = "Chunk World")
	float SpawnChunkDelay = 0.02f;
	UPROPERTY(EditAnywhere, Category = "Chunk World")
	float ClearChunkDelay = 0.02f;

	float CurrentSpawnChunkDelay = 0.f;
	float CurrentClearChunkDelay = 0.f;

	FIntVector2 CurrentPlayerChunk;
	TArray<FIntVector2> VisibleChunks;

	UPROPERTY()
	AVoxelGenerationCharacter* PlayerCharacter = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Chunk World")
	TSubclassOf<AChunk> ChunkClass;

	UPROPERTY(EditAnywhere, Category = "Chunk World")
	int DrawDistance = 5;
};
