#include "Actors/ChunkWorld.h"

#include "Actors/GreedyChunk.h"
#include "Kismet/GameplayStatics.h"
#include "Objects/TerrainGenerator.h"
#include "Structs/ChunkData.h"
#include "Player/Character/VoxelGenerationCharacter.h"

AChunkWorld::AChunkWorld()
{
    PrimaryActorTick.bCanEverTick = true;

    TerrainGenerator = CreateDefaultSubobject<UTerrainGenerator>("TerrainGenerator");
}

void AChunkWorld::BeginPlay()
{
    Super::BeginPlay();

    ChunkSizeSquared = FChunkData::GetChunkSize(this) * FChunkData::GetChunkSize(this);
    
    InitializeWorld();
}

void AChunkWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    
    ChunksPendingGenerationMap.Empty();
    ChunkClearQueue.Empty();
}

void AChunkWorld::InitializeWorld()
{
    PlayerCharacter = Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

    SetTickableWhenPaused(true);
    UGameplayStatics::SetGamePaused(GetWorld(), true);

    CurrentClearChunkDelay = ClearChunkDelay;
    
    GenerateChunksData();
    
    // Force update if player position hasn't changed yet but world just loaded
    if (IsPlayerChunkUpdated() || !PlayerCharacter)
    {
        UpdateChunks();
    }
    // If player position is already set, ensure CurrentPlayerChunk is used
    else
    {
        ActivateVisibleChunks(CurrentPlayerChunk);
        SortVisibleChunksByDistance();
    }
}

void AChunkWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateChunks();
    ProcessChunksMeshGeneration();
    ProcessChunksMeshClear(DeltaTime);
}

void AChunkWorld::GenerateChunksData()
{
    for (int x = 0; x < FChunkData::GetWorldSize(this); ++x)
    {
        for (int y = 0; y < FChunkData::GetWorldSize(this); ++y)
        {
            LoadChunkAtPosition(FIntVector2(x, y));
        }
    }
}

void AChunkWorld::UpdateChunks()
{
    if (!IsPlayerChunkUpdated() && !UGameplayStatics::IsGamePaused(GetWorld())) return;

    TArray<FIntVector2> PreviousVisibleChunks = VisibleChunks;
    VisibleChunks.Empty();

    ActivateVisibleChunks(CurrentPlayerChunk);
    SortVisibleChunksByDistance();
    DeactivatePreviousChunks(PreviousVisibleChunks);
}

bool AChunkWorld::IsInsideDrawDistance(const FIntVector2& ChunkCoordinates, int x, int y)
{
    return x >= ChunkCoordinates.X - DrawDistance && x <= ChunkCoordinates.X + DrawDistance &&
        y >= ChunkCoordinates.Y - DrawDistance && y <= ChunkCoordinates.Y + DrawDistance;
}

void AChunkWorld::ActivateVisibleChunks(const FIntVector2& ChunkCoordinates)
{
    const int LoadDistance = DrawDistance + 1;

    // Populate VisibleChunks in a spiral/radial order for better prioritization
    // Start with the player's chunk
    if (ChunksData.Contains(CurrentPlayerChunk))
    {
        AChunkBase* CenterChunk = ChunksData[CurrentPlayerChunk];
        if (CenterChunk && !CenterChunk->IsMeshInitialized() && !CenterChunk->bIsProcessingMesh)
        {
            EnqueueChunkForGeneration(CenterChunk);
        }
        VisibleChunks.Add(CurrentPlayerChunk);
    }

    // Iterate outwards in rings
    for (int r = 1; r <= LoadDistance; ++r)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                // Only process the outer layer of the current ring
                if (FMath::Abs(dx) != r && FMath::Abs(dy) != r)
                {
                    continue;
                }

                FIntVector2 Coord(CurrentPlayerChunk.X + dx, CurrentPlayerChunk.Y + dy);

                if (FMath::Abs(dx) > LoadDistance || FMath::Abs(dy) > LoadDistance) continue;
                
                if (IsInsideDrawDistance(CurrentPlayerChunk, Coord.X, Coord.Y))
                {
                    if (AChunkBase* LoadedChunk = ChunksData.FindRef(Coord))
                    {
                        if (!VisibleChunks.Contains(Coord))
                        {
                            VisibleChunks.Add(Coord);
                        }
                        
                        if (!LoadedChunk->IsMeshInitialized() && !LoadedChunk->bIsProcessingMesh)
                        {
                            EnqueueChunkForGeneration(LoadedChunk);
                        }
                    }
                }
            }
        }
    }
}

void AChunkWorld::DeactivatePreviousChunks(const TArray<FIntVector2>& PreviousVisibleChunks)
{
    for (const FIntVector2& Coord : PreviousVisibleChunks)
    {
        if (!VisibleChunks.Contains(Coord))
        {
            if (AChunkBase* Chunk = ChunksData[Coord])
            {
                EnqueueChunkForClearing(Chunk);
            }
        }
    }
}

bool AChunkWorld::IsPlayerChunkUpdated()
{
    if (!PlayerCharacter)
    {
        PlayerCharacter = Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    }
    if (!PlayerCharacter) return false;

    const FIntVector2 PlayerChunk = FChunkData::GetChunkPosition(this, PlayerCharacter->GetActorLocation());
    if (PlayerChunk != CurrentPlayerChunk)
    {
        CurrentPlayerChunk = PlayerChunk;
        return true;
    }
    return false;
}

AChunkBase* AChunkWorld::LoadChunkAtPosition(const FIntVector2& ChunkCoordinates)
{
    if (!TerrainGenerator || !TerrainGenerator->IsInitialized()) return nullptr;

    FVector ChunkWorldPosition(
        ChunkCoordinates.X * FChunkData::GetChunkSize(this) * FChunkData::GetScaledBlockSize(this),
        ChunkCoordinates.Y * FChunkData::GetChunkSize(this) * FChunkData::GetScaledBlockSize(this),
        0.0f);

    // Spawn actor
    AChunkBase* Chunk = GetWorld()->SpawnActor<AChunkBase>(ChunkClass, ChunkWorldPosition, FRotator::ZeroRotator);
    if (Chunk)
    {
        Chunk->ChunkPosition = ChunkCoordinates;
        Chunk->SetParentWorld(this);

        // Generate Column Data 
        TArray<FChunkColumn> Columns;
        Columns.SetNum(ChunkSizeSquared);

        FIntVector2 LocalChunkPosition = FChunkData::GetChunkPosition(this, ChunkWorldPosition);

        int32 ChunkSize = FChunkData::GetChunkSize(this);

        for (int x = 0; x < ChunkSize; ++x)
        {
            for (int y = 0; y < ChunkSize; ++y)
            {
                int GlobalX = LocalChunkPosition.X * FChunkData::GetChunkSize(this) + x;
                int GlobalY = LocalChunkPosition.Y * FChunkData::GetChunkSize(this) + y;

                // Generate the data for this column
                FChunkColumn ColumnData = TerrainGenerator->GenerateColumnData(GlobalX, GlobalY);

                // Populate the blocks based on the generated data
                TerrainGenerator->PopulateColumnBlocks(ColumnData);

                Columns[FChunkData::GetColumnIndex(this, x, y)] = ColumnData;
            }
        }

        // Decorate the column with foliage
        FRandomStream FoliageRandomStream(Seed + ChunkCoordinates.X * 73856093 ^ ChunkCoordinates.Y * 19349663);
        TerrainGenerator->DecorateChunkWithFoliage(
        Columns, 
        ChunkCoordinates,
        FoliageRandomStream);
        
        Chunk->SetColumns(Columns);

        ChunksData.Add(ChunkCoordinates, Chunk);
        UE_LOG(LogTemp, Verbose, TEXT("Loaded chunk at %d, %d"), ChunkCoordinates.X, ChunkCoordinates.Y);
    }

    return Chunk;
}

void AChunkWorld::ProcessChunksMeshGeneration()
{
    // Iterate through VisibleChunks (which should be sorted by proximity)
    // to pick the next chunk to generate from the ChunksPendingGenerationMap.
    for (const FIntVector2& ChunkCoord : VisibleChunks)
    {
        // All task slots are busy
        if (RunningMeshTasks >= MaxConcurrentMeshTasks)
        {
            break;
        }

        if (AChunkBase** ChunkPtr = ChunksPendingGenerationMap.Find(ChunkCoord))
        {
            AChunkBase* ChunkToProcess = *ChunkPtr;
            if (ChunkToProcess && ChunkToProcess->IsValidLowLevel() && !ChunkToProcess->IsPendingKillPending())
            {
                // Remove from pending map as we are about to process it
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

    // If there are still task slots and chunks in ChunksPendingGenerationMap
    // that were not in VisibleChunks, process them.
    TArray<FIntVector2> KeysToRemove;
    for (auto It = ChunksPendingGenerationMap.CreateIterator(); It && RunningMeshTasks < MaxConcurrentMeshTasks; ++It)
    {
        AChunkBase* ChunkToProcess = It.Value();
        if (ChunkToProcess && ChunkToProcess->IsValidLowLevel() && !ChunkToProcess->IsPendingKillPending())
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
    for (const FIntVector2& Key : KeysToRemove)
    {
        ChunksPendingGenerationMap.Remove(Key);
    }


    UnPauseGameIfChunksLoadingComplete();
}

void AChunkWorld::EnqueueChunkForGeneration(AChunkBase* Chunk)
{
    if (Chunk && !Chunk->bIsProcessingMesh && !Chunk->IsMeshInitialized())
    {
        // Add to map if not already present. If present, it means it's already queued.
        if (!ChunksPendingGenerationMap.Contains(Chunk->ChunkPosition))
        {
            ChunksPendingGenerationMap.Add(Chunk->ChunkPosition, Chunk);
        }
    }
}

void AChunkWorld::ProcessChunksMeshClear(float DeltaTime)
{
    CurrentClearChunkDelay -= DeltaTime;

    if (CurrentClearChunkDelay <= 0.f && !ChunkClearQueue.IsEmpty())
    {
        AChunkBase* Chunk = nullptr;
        if (ChunkClearQueue.Dequeue(Chunk) && Chunk)
        {
            Chunk->ClearMesh();
        }
        CurrentClearChunkDelay = ClearChunkDelay;
    }
}

void AChunkWorld::EnqueueChunkForClearing(AChunkBase* Chunk)
{
    if (Chunk && Chunk->IsMeshInitialized() && !Chunk->bIsProcessingMesh)
    {
        ChunkClearQueue.Enqueue(Chunk);
    }
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
    if (UGameplayStatics::IsGamePaused(GetWorld()) && ChunksPendingGenerationMap.IsEmpty() && RunningMeshTasks == 0)
    {
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        UE_LOG(LogTemp, Log, TEXT("All pending initial chunks processed. Unpausing game."));
    }
}
