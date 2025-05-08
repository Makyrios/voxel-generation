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
    
    ChunkGenerationQueue.Empty();
    ChunkClearQueue.Empty();
}

void AChunkWorld::InitializeWorld()
{
    PlayerCharacter = Cast<AVoxelGenerationCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

    SetTickableWhenPaused(true);
    UGameplayStatics::SetGamePaused(GetWorld(), true);

    CurrentSpawnChunkDelay = SpawnChunkDelay;
    CurrentClearChunkDelay = ClearChunkDelay;
    
    GenerateChunksData();
}

void AChunkWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateChunks();
    ProcessChunksMeshGeneration(DeltaTime);
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
    DeactivatePreviousChunks(PreviousVisibleChunks);
}

bool AChunkWorld::IsInsideDrawDistance(const FIntVector2& ChunkCoordinates, int x, int y)
{
    return x >= ChunkCoordinates.X - DrawDistance && x <= ChunkCoordinates.X + DrawDistance &&
        y >= ChunkCoordinates.Y - DrawDistance && y <= ChunkCoordinates.Y + DrawDistance;
}

void AChunkWorld::ActivateVisibleChunks(const FIntVector2& ChunkCoordinates)
{
    // Load chunks in a visible radius + 1 to hide meshes between neighbor chunks
    int LoadDistance = DrawDistance + 1;

    for (int x = ChunkCoordinates.X - LoadDistance; x <= ChunkCoordinates.X + LoadDistance; ++x)
    {
        for (int y = ChunkCoordinates.Y - LoadDistance; y <= ChunkCoordinates.Y + LoadDistance; ++y)
        {
            FIntVector2 Coord(x, y);

            if (IsInsideDrawDistance(ChunkCoordinates, x, y))
            {
                if (!ChunksData.Contains(Coord)) continue;
                
                AChunkBase* LoadedChunk = ChunksData[Coord];
                if (LoadedChunk && !LoadedChunk->IsMeshInitialized() && !LoadedChunk->bIsProcessingMesh)
                {
                    EnqueueChunkForGeneration(LoadedChunk);
                }
                VisibleChunks.Add(Coord);
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
        Chunk->SetColumns(Columns);

        ChunksData.Add(ChunkCoordinates, Chunk);
        UE_LOG(LogTemp, Verbose, TEXT("Loaded chunk at %d, %d"), ChunkCoordinates.X, ChunkCoordinates.Y);
    }

    return Chunk;
}

void AChunkWorld::ProcessChunksMeshGeneration(float DeltaTime)
{
    CurrentSpawnChunkDelay -= DeltaTime;

    if (CurrentSpawnChunkDelay <= 0.f && !ChunkGenerationQueue.IsEmpty())
    {
        AChunkBase* Chunk = nullptr;
        if (ChunkGenerationQueue.Dequeue(Chunk) && Chunk)
        {
            Chunk->RegenerateMeshAsync();
        }
        CurrentSpawnChunkDelay = SpawnChunkDelay;
    }

    UnPauseGameIfChunksLoadingComplete();
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

void AChunkWorld::EnqueueChunkForGeneration(AChunkBase* Chunk)
{
    Chunk->bIsProcessingMesh = true;
    ChunkGenerationQueue.Enqueue(Chunk);
}

void AChunkWorld::EnqueueChunkForClearing(AChunkBase* Chunk)
{
    ChunkClearQueue.Enqueue(Chunk);
}

void AChunkWorld::UnPauseGameIfChunksLoadingComplete() const
{
    if (UGameplayStatics::IsGamePaused(GetWorld()) && ChunkGenerationQueue.IsEmpty())
    {
        UGameplayStatics::SetGamePaused(GetWorld(), false);
    }
}
