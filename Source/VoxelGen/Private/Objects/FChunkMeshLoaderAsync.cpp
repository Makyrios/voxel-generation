#include "Objects/FChunkMeshLoaderAsync.h"

#include "Actors/ChunkBase.h"

FChunkMeshLoaderAsync::FChunkMeshLoaderAsync(AChunkBase* InChunk) : ChunkPtr(InChunk)
{
}

TStatId FChunkMeshLoaderAsync::GetStatId()
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FChunkMeshLoaderAsync, STATGROUP_ThreadPoolAsyncTasks);
}

void FChunkMeshLoaderAsync::DoWork()
{
	if (AChunkBase* Chunk = ChunkPtr.Get())
	{
		if (Chunk->IsValidLowLevel() && !Chunk->IsPendingKillPending() && Chunk->GetWorld())
		{
			Chunk->RegenerateMesh();
			
		}
	}
}