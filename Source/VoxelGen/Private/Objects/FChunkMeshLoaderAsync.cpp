#include "Objects/FChunkMeshLoaderAsync.h"

#include "Actors/Chunk.h"

FChunkMeshLoaderAsync::FChunkMeshLoaderAsync(AChunk* InChunk) : Chunk(InChunk)
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