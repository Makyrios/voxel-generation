// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChunkMeshData.generated.h"

USTRUCT()
struct VOXELGEN_API FChunkMeshData
{
	GENERATED_BODY()

public:
	TArray<FVector> Vertices;
	TArray<int> Triangles;
	TArray<FVector2D> UV;

public:
	void Clear();
};
