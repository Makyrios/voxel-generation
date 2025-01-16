#include "Objects/FChunkMeshLoaderAsync.h"

#include "Actors/ChunkBase.h"

FChunkMeshLoaderAsync::FChunkMeshLoaderAsync(AChunkBase* InChunk) : Chunk(InChunk)
{
}

TStatId FChunkMeshLoaderAsync::GetStatId()
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FChunkMeshLoaderAsync, STATGROUP_ThreadPoolAsyncTasks);
}

void FChunkMeshLoaderAsync::DoWork()
{
	if (Chunk)
	{
		Chunk->RegenerateMesh();
	}
}