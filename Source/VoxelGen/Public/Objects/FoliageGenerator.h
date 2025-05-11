// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGen/Enums.h"
#include "FoliageGenerator.generated.h"

struct FChunkColumn;
struct FBiomeSettings;

UCLASS()
class VOXELGEN_API UFoliageGenerator : public UObject
{
	GENERATED_BODY()

public:
	UFoliageGenerator();
	
	bool AttemptPlaceFoliageAt(
		TArray<FChunkColumn>& ChunkColumnsData,
		int LocalX, int LocalY,
		const FBiomeSettings* BiomeInfo,
		const FRandomStream& ColumnSpecificStream,
		int ChunkSize, int ChunkHeight
	);

private:
	void GenerateOakTree(TArray<FChunkColumn>& ChunkColumnsData, const FIntVector& TreeBaseLocalPosInChunk,
		int Height, bool bLargeVariant, FRandomStream& TreeInstanceStream,
		int ChunkSize, int ChunkHeight);
	void GenerateBirchTree(TArray<FChunkColumn>& ChunkColumnsData, const FIntVector& TreeBaseLocalPosInChunk,
		int Height, bool bLargeVariant, FRandomStream& TreeInstanceStream,
		int ChunkSize, int ChunkHeight);
	void GenerateCactus(TArray<FChunkColumn>& ChunkColumnsData, const FIntVector& CactusBaseLocalPosInChunk,
		int Height, FRandomStream& TreeInstanceStream,
		int ChunkSize, int ChunkHeight);
	void GenerateGrass(TArray<EBlock>& Blocks, int BaseZ, int ChunkHeight);
	
	void SetBlockInSingleColumnArray(TArray<EBlock>& Blocks, int Z, EBlock BlockType, int ChunkHeight) const;
	void SetBlockInChunkColumns(
		TArray<FChunkColumn>& ChunkColumnsData,
		int LocalX, int LocalY, int LocalZ,
		EBlock BlockType,
		int ChunkSize, int ChunkHeight) const;
	
};
