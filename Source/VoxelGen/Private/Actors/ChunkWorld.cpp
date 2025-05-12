#include "Actors/ChunkWorld.h"

#include "Actors/ChunkBase.h"
#include "Kismet/GameplayStatics.h"
#include "Objects/TerrainGenerator.h"
#include "Structs/ChunkData.h"
#include "Player/Character/VoxelGenerationCharacter.h"
#include "HAL/PlatformMisc.h"
#include "Async/Async.h"
#include "Logging/LogMacros.h"


float DistSquared(const FIntVector2& A, const FIntVector2& B)
{
    return FMath::Square(A.X - B.X) + FMath::Square(A.Y - B.Y);
}

AChunkWorld::AChunkWorld()
{
    PrimaryActorTick.bCanEverTick = true;
    TerrainGenerator = CreateDefaultSubobject<UTerrainGenerator>("TerrainGenerator");
}

void AChunkWorld::BeginPlay()
{
    Super::BeginPlay();

    InitializeWorld();

    bWorldInitialized = true;
}

void AChunkWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    TArray<FIntVector2> Keys;
    ChunksData.GetKeys(Keys);
    for (const FIntVector2& Key : Keys)
    {
        DestroyChunkActor(Key);
    }

    ChunksData.Empty();
    ChunksPendingGenerationMap.Empty();
    VisibleChunks.Empty();

    RunningMeshTasks = 0;
}

void AChunkWorld::InitializeWorld()
{
    LoadDistance = DrawDistance + 1;

    Seed = FChunkData::GetSeed(this);
    ChunkSize = FChunkData::GetChunkSize(this);
    ScaledBlockSize = FChunkData::GetScaledBlockSize(this);
    
    PlayerCharacter = Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!PlayerCharacter) return;

    if (!TerrainGenerator) {
        return;
    }
    
    if (!TerrainGenerator->IsNoiseInitialized())
    {
        FTimerHandle WaitTimer;
        GetWorldTimerManager().SetTimer(WaitTimer, this, &AChunkWorld::InitializeWorld, 0.1f, false);
        return;
    }

    SetTickableWhenPaused(true);
    UGameplayStatics::SetGamePaused(GetWorld(), true);
}

void AChunkWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (IsPlayerChunkUpdated())
    {
        UpdateChunksData();
        UpdateChunksForGeneration();
        SortVisibleChunksByDistance();
    }
    ProcessChunksMeshGeneration();
}

void AChunkWorld::RegenerateWorld()
{
    for (const auto& Pair : ChunksData)
    {
        DestroyChunkActor(Pair.Key);
    }
    
    ChunksData.Empty();
    SavedChunkColumns.Empty();
    ChunksPendingGenerationMap.Empty();
    VisibleChunks.Empty();

    Seed = FChunkData::GetSeed(this);
    ChunkSize = FChunkData::GetChunkSize(this);
    ScaledBlockSize = FChunkData::GetScaledBlockSize(this);
    if (TerrainGenerator)
    {
        TerrainGenerator->UpdateSeed(Seed);
    }
    
    UpdateChunksData();
    UpdateChunksForGeneration();
    SortVisibleChunksByDistance();
    ProcessChunksMeshGeneration();
}

void AChunkWorld::UpdateChunksForGeneration()
{
    TArray<FIntVector2> NewVisibleChunks;
    NewVisibleChunks.Reserve( (DrawDistance * 2 + 1) * (DrawDistance * 2 + 1) );

    // 1. Determine the new visible chunks based on the current player chunk
    if (auto CenterChunkPtr = ChunksData.Find(CurrentPlayerChunk))
    {
        if(AChunkBase* CenterChunk = *CenterChunkPtr)
         {
            NewVisibleChunks.Add(CurrentPlayerChunk);
            if (!CenterChunk->IsMeshInitialized() && !CenterChunk->bIsProcessingMesh)
            {
                ChunksPendingGenerationMap.FindOrAdd(CurrentPlayerChunk, CenterChunk);
            }
         }
    }

    // 2. Iterate over the surrounding chunks within the DrawDistance
    for (int r = 1; r <= DrawDistance; ++r)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                if (FMath::Abs(dx) != r && FMath::Abs(dy) != r)
                {
                    continue;
                }

                FIntVector2 Coord(CurrentPlayerChunk.X + dx, CurrentPlayerChunk.Y + dy);

                if (auto LoadedChunkPtr = ChunksData.Find(Coord))
                {
                    AChunkBase* LoadedChunk = *LoadedChunkPtr;
                    if (LoadedChunk)
                    {
                        NewVisibleChunks.Add(Coord);

                        if (!LoadedChunk->IsMeshInitialized() && !LoadedChunk->bIsProcessingMesh)
                        {
                            ChunksPendingGenerationMap.FindOrAdd(Coord, LoadedChunk);
                        }
                    }
                }
            }
        }
    }

    // Replace the old VisibleChunks list with the newly ordered one
    VisibleChunks = MoveTemp(NewVisibleChunks);

    // Remove entries for chunks that are no longer visible
    // or whose actors might have been destroyed
    TArray<FIntVector2> KeysToRemoveFromPending;
    for(const auto& Pair : ChunksPendingGenerationMap)
    {
        if (!VisibleChunks.Contains(Pair.Key) || !IsValid(Pair.Value))
        {
            KeysToRemoveFromPending.Add(Pair.Key);
        }
    }
    for(const FIntVector2& Key : KeysToRemoveFromPending)
    {
        ChunksPendingGenerationMap.Remove(Key);
    }
}

bool AChunkWorld::IsPlayerChunkUpdated()
{
    if (!PlayerCharacter) return false;

    const FIntVector2 PlayerChunk = FChunkData::GetChunkPosition(this, PlayerCharacter->GetActorLocation());

    if (PlayerChunk != CurrentPlayerChunk)
    {
        CurrentPlayerChunk = PlayerChunk;
        return true;
    }
    return false;
}

void AChunkWorld::UpdateChunksData()
{
    if (!TerrainGenerator) return;
    
    TSet<FIntVector2> RequiredChunks;
    TSet<FIntVector2> ChunksToRemove;
    TArray<FIntVector2> ChunksToAdd;

    // 1. Determine Required Chunks based on LoadDistance
    for (int32 y = CurrentPlayerChunk.Y - LoadDistance; y <= CurrentPlayerChunk.Y + LoadDistance; ++y)
    {
        for (int32 x = CurrentPlayerChunk.X - LoadDistance; x <= CurrentPlayerChunk.X + LoadDistance; ++x)
        {
             RequiredChunks.Add(FIntVector2(x, y));
        }
    }

     // 2. Identify Chunks to Remove (currently loaded but not required)
     for (const auto& Pair : ChunksData)
     {
         if (!RequiredChunks.Contains(Pair.Key))
         {
             ChunksToRemove.Add(Pair.Key);
         }
     }

     // 3. Identify Chunks to Add (required but not currently loaded)
     for (const FIntVector2& RequiredCoord : RequiredChunks)
     {
         if (!ChunksData.Contains(RequiredCoord))
         {
             ChunksToAdd.Add(RequiredCoord);
         }
     }

     // 4. Destroy Chunks that are out of LoadDistance
     for (const FIntVector2& CoordToRemove : ChunksToRemove)
     {
         DestroyChunkActor(CoordToRemove);
     }

     // 5. Spawn New Chunks that entered LoadDistance
      ChunksToAdd.Sort([this](const FIntVector2& A, const FIntVector2& B) {
          return DistSquared(CurrentPlayerChunk, A) < DistSquared(CurrentPlayerChunk, B);
      });

     for (const FIntVector2& CoordToAdd : ChunksToAdd)
     {
         if (!ChunksData.Contains(CoordToAdd))
         {
             LoadChunkAtPosition(CoordToAdd);
         }
     }
}


AChunkBase* AChunkWorld::LoadChunkAtPosition(const FIntVector2& ChunkCoordinates)
{
    if (!TerrainGenerator) return nullptr;

    if (AChunkBase* Restored = TryRestoreSavedChunk(ChunkCoordinates))
    {
        return Restored;
    }

    if (AChunkBase* Existing = GetExistingChunk(ChunkCoordinates))
    {
        return Existing;
    }

    AChunkBase* NewChunk = CreateAndInitializeChunk(ChunkCoordinates);
    return NewChunk;
}

AChunkBase* AChunkWorld::TryRestoreSavedChunk(const FIntVector2& ChunkCoordinates)
{
    if (!SavedChunkColumns.Contains(ChunkCoordinates))
    {
        return nullptr;
    }

    TArray<FChunkColumn> Columns = SavedChunkColumns.FindAndRemoveChecked(ChunkCoordinates);
    AChunkBase* Chunk = SpawnChunkActorAt(ChunkCoordinates);
    if (Chunk)
    {
        Chunk->SetColumns(MoveTemp(Columns));
        ChunksData.Add(ChunkCoordinates, Chunk);
    }
    return Chunk;
}

AChunkBase* AChunkWorld::GetExistingChunk(const FIntVector2& ChunkCoordinates) const
{
    if (ChunksData.Contains(ChunkCoordinates))
    {
        return ChunksData[ChunkCoordinates];
    }
    return nullptr;
}

AChunkBase* AChunkWorld::CreateAndInitializeChunk(const FIntVector2& ChunkCoordinates)
{
    if (!ChunkClass) return nullptr;

    AChunkBase* Chunk = SpawnChunkActorAt(ChunkCoordinates);
    if (!Chunk) return nullptr;

    // Generate column data
    const int32 Count = ChunkSize * ChunkSize;
    TArray<FChunkColumn> Columns;
    Columns.SetNum(Count);

    for (int32 x = 0; x < ChunkSize; ++x)
    {
        for (int32 y = 0; y < ChunkSize; ++y)
        {
            int32 idx = FChunkData::GetColumnIndexFromLocal(x, y, ChunkSize);
            int32 gx  = ChunkCoordinates.X * ChunkSize + x;
            int32 gy  = ChunkCoordinates.Y * ChunkSize + y;

            Columns[idx] = TerrainGenerator->GenerateColumnData(gx, gy);
            TerrainGenerator->PopulateColumnBlocks(Columns[idx]);
        }
    }

    FRandomStream Stream(Seed + ChunkCoordinates.X * 73856093 ^ ChunkCoordinates.Y * 19349663);
    TerrainGenerator->DecorateChunkWithFoliage(Columns, ChunkCoordinates, Stream);

    Chunk->SetColumns(Columns);
    ChunksData.Add(ChunkCoordinates, Chunk);

    return Chunk;
}

AChunkBase* AChunkWorld::SpawnChunkActorAt(const FIntVector2& ChunkCoordinates)
{
    FVector Pos(
        ChunkCoordinates.X * ChunkSize * ScaledBlockSize,
        ChunkCoordinates.Y * ChunkSize * ScaledBlockSize,
        0.0f
    );
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    AChunkBase* Chunk = GetWorld()->SpawnActor<AChunkBase>(
        ChunkClass, Pos, FRotator::ZeroRotator, SpawnParams 
    );
    if (Chunk)
    {
        Chunk->ChunkPosition = ChunkCoordinates;
        Chunk->SetParentWorld(this);
    }
    return Chunk;
}

void AChunkWorld::DestroyChunkActor(const FIntVector2& ChunkCoordinates)
{
    if (AChunkBase* ChunkToDestroy = ChunksData.FindRef(ChunkCoordinates))
    {
        SavedChunkColumns.Add(ChunkCoordinates, ChunkToDestroy->GetColumns());
        
        if (IsValid(ChunkToDestroy))
        {
            ChunkToDestroy->Destroy();
        }
    }

    ChunksData.Remove(ChunkCoordinates);
    ChunksPendingGenerationMap.Remove(ChunkCoordinates);
    VisibleChunks.Remove(ChunkCoordinates);
}

// Processes the mesh generation queue based on VisibleChunks order.
void AChunkWorld::ProcessChunksMeshGeneration()
{
    for (const FIntVector2& ChunkCoord : VisibleChunks)
    {
        if (RunningMeshTasks >= MaxConcurrentMeshTasks) break;

        if (auto* ChunkPtr = ChunksPendingGenerationMap.Find(ChunkCoord))
        {
            AChunkBase* ChunkToProcess = *ChunkPtr;
            if (ChunkToProcess && IsValid(ChunkToProcess) && !ChunkToProcess->IsPendingKillPending() && !ChunkToProcess->bIsProcessingMesh)
            {
                ChunksPendingGenerationMap.Remove(ChunkCoord);
                ChunkToProcess->bIsProcessingMesh = true;
                ++RunningMeshTasks;
                ChunkToProcess->RegenerateMeshAsync();
            }
            else
            {
                ChunksPendingGenerationMap.Remove(ChunkCoord);
            }
        }
    }

    // If slots are still available, process any remaining chunks in the pending map
    if (RunningMeshTasks < MaxConcurrentMeshTasks && ChunksPendingGenerationMap.Num() > 0)
    {
        TArray<FIntVector2> KeysToRemove;
        for (auto It = ChunksPendingGenerationMap.CreateIterator(); It && RunningMeshTasks < MaxConcurrentMeshTasks; ++It)
        {
            AChunkBase* ChunkToProcess = It.Value();
            if (ChunkToProcess && IsValid(ChunkToProcess) && !ChunkToProcess->IsPendingKillPending() && !ChunkToProcess->bIsProcessingMesh)
            {
                ChunkToProcess->bIsProcessingMesh = true;
                ++RunningMeshTasks;
                ChunkToProcess->RegenerateMeshAsync();
                KeysToRemove.Add(It.Key());
            }
            else
            {
                KeysToRemove.Add(It.Key());
            }
        }
        
        // Remove processed/invalid chunks from the map
        for (const FIntVector2& Key : KeysToRemove)
        {
            ChunksPendingGenerationMap.Remove(Key);
        }
    }

    UnPauseGameIfChunksLoadingComplete();
}


void AChunkWorld::SortVisibleChunksByDistance()
{
    if (!PlayerCharacter) return;
    FVector PlayerLocation = PlayerCharacter->GetActorLocation();
    
    VisibleChunks.Sort([this, PlayerLocation](const FIntVector2& A, const FIntVector2& B)
    {
        FVector WorldPosA(
            A.X * FChunkData::GetChunkSize(this) * FChunkData::GetScaledBlockSize(this),
            A.Y * FChunkData::GetChunkSize(this) * FChunkData::GetScaledBlockSize(this),
            0.0f);
        FVector WorldPosB(
            B.X * FChunkData::GetChunkSize(this) * FChunkData::GetScaledBlockSize(this),
            B.Y * FChunkData::GetChunkSize(this) * FChunkData::GetScaledBlockSize(this),
            0.0f);
            
        return FVector::DistSquared(PlayerLocation, WorldPosA) < FVector::DistSquared(PlayerLocation, WorldPosB);
    });
}

void AChunkWorld::UnPauseGameIfChunksLoadingComplete() const
{
    if (UGameplayStatics::IsGamePaused(GetWorld()) && !ChunksPendingGenerationMap.IsEmpty())
    {
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        UE_LOG(LogTemp, Log, TEXT("Initial chunk generation complete. Unpausing game."));
    }
}

bool AChunkWorld::IsWithinLoadDistance(const FIntVector2& ChunkCoordinates) const
{
    float DistSq = DistSquared(CurrentPlayerChunk, ChunkCoordinates);
    return DistSq <= (LoadDistance * LoadDistance);
}

bool AChunkWorld::IsWithinDrawDistance(const FIntVector2& ChunkCoordinates) const
{
     float DistSq = DistSquared(CurrentPlayerChunk, ChunkCoordinates);
     return DistSq <= (DrawDistance * DrawDistance);
}