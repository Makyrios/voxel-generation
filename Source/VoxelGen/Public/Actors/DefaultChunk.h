// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChunkBase.h"
#include "DefaultChunk.generated.h"

UCLASS()
class VOXELGEN_API ADefaultChunk final : public AChunkBase
{
	GENERATED_BODY()

public:
	ADefaultChunk();

protected:
	virtual void BeginPlay() override;

	virtual void GenerateMesh() override;

private:
	void CreateCrossPlanes(const FIntVector& CurrentBlockPos, EBlock Block, const FBlockSettings*& BlockSettings);
	void CreateCubePlanes(const FIntVector& CurrentBlockPos, EBlock Block, const FBlockSettings*& BlockSettings);
	
	void CreateFace(EDirection Direction, const FIntVector& Position, EBlock BlockType, const FBlockSettings* BlockProperties);
	TArray<FVector> GetFaceVerticies(EDirection Direction, const FVector& WorldPosition) const;
	FVector GetNormal(EDirection Direction);
};
