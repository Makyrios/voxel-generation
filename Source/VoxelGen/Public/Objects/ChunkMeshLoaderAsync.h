#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "Actors/ChunkBase.h"

class FChunkMeshLoaderAsync : public FNonAbandonableTask
{
	
public:
	FChunkMeshLoaderAsync(AChunkBase* InChunk);

	static TStatId GetStatId();
	void DoWork();

private:
	TWeakObjectPtr<AChunkBase> ChunkPtr;
};
