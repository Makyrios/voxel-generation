// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChunkBase.h"
#include "GreedyChunk.generated.h"

UCLASS()
class VOXELGEN_API AGreedyChunk final : public AChunkBase
{
	GENERATED_BODY()

	struct FMask
	{
		EBlock Block;
		int Normal; // Direction of the face (1 for positive, -1 for negative, 0 for none)
	};

private:
	virtual void GenerateMesh() override;
	
	void CreateQuad(FMask Mask, FIntVector AxisMask, int Width, int Height,
		FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4);
	
	bool CompareMask(FMask M1, FMask M2) const;
};
