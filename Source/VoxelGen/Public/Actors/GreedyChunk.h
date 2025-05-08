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
		// const FBlockSettings* BlockProperties = nullptr;
		EBlock Block = EBlock::Air;
		int8 Normal = 0; // -1 or 1 for direction relative to sweep

		FMask() = default;
		FMask(EBlock InBlock, int8 InNormal)
			: Block(InBlock), Normal(InNormal) {}
	};

private:
	virtual void GenerateMesh() override;
	
	void CreateQuad(const FMask& Mask, const FIntVector& AxisMask, int Width, int Height,
		const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4);
	
	bool CompareMask(const FMask& M1, const FMask& M2) const;
};
