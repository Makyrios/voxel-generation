#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "Actors/Chunk.h"

class FChunkMeshLoaderAsync : public FNonAbandonableTask
{
public:
	FChunkMeshLoaderAsync(AChunk* InChunk);

	static TStatId GetStatId();
	void DoWork();

private:
	AChunk* Chunk;
};
