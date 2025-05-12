#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkWorld.generated.h"

struct FChunkColumn;
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

    const TMap<FIntVector2, TObjectPtr<AChunkBase>>& GetChunksData() const { return ChunksData; }
    void NotifyMeshTaskCompleted() { --RunningMeshTasks; }

    UFUNCTION(BlueprintCallable)
    void RegenerateWorld();


protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

private:
    // Initialization
    void InitializeWorld();

    // Update Logic
    void UpdateChunksForGeneration();
    void UpdateChunksData(); 
    void ProcessChunksMeshGeneration();

    // Chunk Management
    bool IsPlayerChunkUpdated();
    AChunkBase* TryRestoreSavedChunk(const FIntVector2& ChunkCoordinates);
    AChunkBase* GetExistingChunk(const FIntVector2& ChunkCoordinates) const;
    AChunkBase* CreateAndInitializeChunk(const FIntVector2& ChunkCoordinates);
    AChunkBase* SpawnChunkActorAt(const FIntVector2& ChunkCoordinates);
    AChunkBase* LoadChunkAtPosition(const FIntVector2& ChunkCoordinates);
    void DestroyChunkActor(const FIntVector2& ChunkCoordinates);

    // Helper Functions
    void SortVisibleChunksByDistance();
    void UnPauseGameIfChunksLoadingComplete() const;
    bool IsWithinLoadDistance(const FIntVector2& ChunkCoordinates) const;
    bool IsWithinDrawDistance(const FIntVector2& ChunkCoordinates) const;

public:
    UPROPERTY(EditAnywhere, Category = "Settings|Chunk World", meta = (ClampMin = "0", UIMin = "0"))
    int32 DrawDistance = 5;
    int32 LoadDistance = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Chunk World")
    int32 Seed = 1000;
    
private:
    // Performance and Utility
    UPROPERTY(EditAnywhere, Category = "Performance", meta = (ClampMin = "1", UIMin = "1"))
    int32 MaxConcurrentMeshTasks = FPlatformMisc::NumberOfCores();

    // Components
    UPROPERTY(EditAnywhere, Category = "Components")
    TObjectPtr<UTerrainGenerator> TerrainGenerator;

    UPROPERTY(EditAnywhere, Category = "Settings|Chunk World")
    TSubclassOf<AChunkBase> ChunkClass;

    // Runtime Data
    TMap<FIntVector2, TObjectPtr<AChunkBase>> ChunksData;
    TMap<FIntVector2, TArray<FChunkColumn>> SavedChunkColumns;
    TMap<FIntVector2, TObjectPtr<AChunkBase>> ChunksPendingGenerationMap;

    TArray<FIntVector2> VisibleChunks;
    FIntVector2 CurrentPlayerChunk;

    UPROPERTY()
    TObjectPtr<AVoxelGenerationCharacter> PlayerCharacter = nullptr;

    // Cached Values
    int32 ChunkSize = 0;
    float ScaledBlockSize = 0.f;

    // State Tracking
    bool bWorldInitialized = false;
    std::atomic<int32> RunningMeshTasks = 0;
};