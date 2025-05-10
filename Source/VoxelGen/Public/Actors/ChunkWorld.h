// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkWorld.generated.h"

struct FBiomeWeight;
class AChunkBase;
class AVoxelGenerationCharacter;
class UTerrainGenerator;

UCLASS()
class VOXELGEN_API AChunkWorld : public AActor
{
	GENERATED_BODY()

public:    
	AChunkWorld();
	void InitializeWorld();

	const TMap<FIntVector2, AChunkBase*>& GetChunksData() const { return ChunksData; }

	void NotifyMeshTaskCompleted() { --RunningMeshTasks; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	void UpdateChunks();
	void ProcessChunksMeshGeneration(float DeltaTime);
	void ProcessChunksMeshClear(float DeltaTime);

	bool IsPlayerChunkUpdated();
	AChunkBase* LoadChunkAtPosition(const FIntVector2& ChunkCoordinates);

	void GenerateChunksData();
	
	void ActivateVisibleChunks(const FIntVector2& ChunkCoordinates);
	bool IsInsideDrawDistance(const FIntVector2& ChunkCoordinates, int x, int y);
	void DeactivatePreviousChunks(const TArray<FIntVector2>& PreviousVisibleChunks);

	void EnqueueChunkForGeneration(AChunkBase* Chunk);
	void EnqueueChunkForClearing(AChunkBase* Chunk);
	
	void SortVisibleChunksByDistance();

	void UnPauseGameIfChunksLoadingComplete() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Chunk World")
	int32 Seed = 1000;

private:
	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UTerrainGenerator> TerrainGenerator;
	
	TMap<FIntVector2, AChunkBase*> ChunksData;
	TMap<FIntVector2, AChunkBase*> ChunksPendingGenerationMap;
	
	TQueue<AChunkBase*> ChunkClearQueue;

	UPROPERTY(EditAnywhere, Category = "Settings|Chunk World")
	float ClearChunkDelay = 0.002f;
	
	float CurrentClearChunkDelay = 0.f;
	
	UPROPERTY(EditAnywhere, Category = "Settings|Chunk World", meta = (ClampMin = "1"))
	int32 MaxConcurrentMeshTasks = FPlatformMisc::NumberOfCores();
	std::atomic<int32> RunningMeshTasks = 0;

	FIntVector2 CurrentPlayerChunk;
	TArray<FIntVector2> VisibleChunks;

	UPROPERTY()
	AVoxelGenerationCharacter* PlayerCharacter = nullptr;

	UPROPERTY(EditAnywhere, Category = "Settings|Chunk World")
	TSubclassOf<AChunkBase> ChunkClass;

	UPROPERTY(EditAnywhere, Category = "Settings|Chunk World")
	int DrawDistance = 5;

	float ChunkSizeSquared;
};
