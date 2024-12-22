// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ChunkData.generated.h"

USTRUCT()
struct FChunkData
{
	GENERATED_BODY()

public:
	static constexpr int32 ChunkSize = 20;
	static constexpr float BlockSize = 100.f;
	static constexpr float BlockScale = 0.5f;
	static constexpr int32 WorldSizeInChunks = 16;

public:
	static FVector GetLocalBlockPosition(const FVector& WorldBlockPosition)
	{
		return FVector(static_cast<int>(WorldBlockPosition.X) % ChunkSize, static_cast<int>(WorldBlockPosition.Y) % ChunkSize, static_cast<int>(WorldBlockPosition.Z) % ChunkSize);
	}

	static FVector ConvertToWorldBlockPosition(const FVector& WorldPosition)
	{
		FVector3d BlockPosition3D = WorldPosition / (BlockSize * BlockScale);
		return FVector3d(FMath::FloorToDouble(BlockPosition3D.X), FMath::FloorToDouble(BlockPosition3D.Y), FMath::FloorToDouble(BlockPosition3D.Z));
	}
};
