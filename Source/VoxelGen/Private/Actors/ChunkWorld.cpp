// ChunkWorld.cpp
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

    SetTickableWhenPaused(true);
    UGameplayStatics::SetGamePaused(GetWorld(), true);

    CurrentSpawnChunkDelay = SpawnChunkDelay;
    CurrentClearChunkDelay = ClearChunkDelay;
}

void AChunkWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateChunks();
    ProcessChunksMeshGeneration(DeltaTime);
    ProcessChunksMeshClear(DeltaTime);
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
    int LoadDistance = DrawDistance + 1;

    for (int x = ChunkCoordinates.X - LoadDistance; x <= ChunkCoordinates.X + LoadDistance; ++x)
    {
        for (int y = ChunkCoordinates.Y - LoadDistance; y <= ChunkCoordinates.Y + LoadDistance; ++y)
        {
            FIntVector2 Coord(x, y);

            if (!ChunksData.Contains(Coord))
            {
                LoadChunkAtPosition(Coord);
            }

            if (IsInsideDrawDistance(ChunkCoordinates, x, y))
            {
                AChunk* LoadedChunk = ChunksData[Coord];
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
            if (AChunk* Chunk = ChunksData[Coord])
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
    FVector ChunkWorldPosition(
        ChunkCoordinates.X * FChunkData::ChunkSize * FChunkData::BlockScaledSize,
        ChunkCoordinates.Y * FChunkData::ChunkSize * FChunkData::BlockScaledSize,
        0.0f);

    AChunk* Chunk = GetWorld()->SpawnActor<AChunk>(ChunkClass, ChunkWorldPosition, FRotator::ZeroRotator);
    if (Chunk)
    {
        Chunk->ChunkPosition = ChunkCoordinates;
        Chunk->GenerateBlocks(ChunkWorldPosition);
        Chunk->SetParentWorld(this);
        ChunksData.Add(ChunkCoordinates, Chunk);
    }

    return Chunk;
}

void AChunkWorld::ProcessChunksMeshGeneration(float DeltaTime)
{
    CurrentSpawnChunkDelay -= DeltaTime;

    if (CurrentSpawnChunkDelay <= 0.f && !ChunkGenerationQueue.IsEmpty())
    {
        AChunk* Chunk = nullptr;
        if (ChunkGenerationQueue.Dequeue(Chunk) && Chunk)
        {
            Chunk->RegenerateMeshAsync();
        }
        CurrentSpawnChunkDelay = SpawnChunkDelay;
    }

    PauseGameIfChunksLoadingComplete();
}

void AChunkWorld::ProcessChunksMeshClear(float DeltaTime)
{
    CurrentClearChunkDelay -= DeltaTime;

    if (CurrentClearChunkDelay <= 0.f && !ChunkClearQueue.IsEmpty())
    {
        AChunk* Chunk = nullptr;
        if (ChunkClearQueue.Dequeue(Chunk) && Chunk)
        {
            Chunk->ClearMesh();
        }
        CurrentClearChunkDelay = ClearChunkDelay;
    }
}

void AChunkWorld::EnqueueChunkForGeneration(AChunk* Chunk)
{
    Chunk->bIsProcessingMesh = true;
    ChunkGenerationQueue.Enqueue(Chunk);
}

void AChunkWorld::EnqueueChunkForClearing(AChunk* Chunk)
{
    ChunkClearQueue.Enqueue(Chunk);
}

void AChunkWorld::PauseGameIfChunksLoadingComplete() const
{
    if (UGameplayStatics::IsGamePaused(GetWorld()) && ChunkGenerationQueue.IsEmpty())
    {
        UGameplayStatics::SetGamePaused(GetWorld(), false);
    }
}
