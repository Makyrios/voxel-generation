// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ChunkData.generated.h"

USTRUCT()
struct FChunkData
{
	GENERATED_BODY()

public:
	static constexpr int32 ChunkSize = 16;
	static constexpr float BlockSize = 100.f;
	static constexpr float BlockScale = 0.5f;
	static constexpr float BlockScaledSize = BlockSize * BlockScale;
	static constexpr int32 WorldSizeInChunks = 500;

public:
	static FIntVector GetLocalBlockPosition(const FIntVector& WorldBlockPosition)
	{
		return FIntVector(
		(WorldBlockPosition.X % ChunkSize + ChunkSize) % ChunkSize,
		(WorldBlockPosition.Y % ChunkSize + ChunkSize) % ChunkSize,
		(WorldBlockPosition.Z % ChunkSize + ChunkSize) % ChunkSize
	);
	}

	static FIntVector GetWorldBlockPosition(const FVector& WorldPosition)
	{
		FVector BlockPosition3D = WorldPosition / (BlockScaledSize);
		return FIntVector(
			FMath::FloorToInt(BlockPosition3D.X),
			FMath::FloorToInt(BlockPosition3D.Y),
			FMath::FloorToInt(BlockPosition3D.Z)
		);
	}

	static FIntVector2 GetChunkContainingBlockPosition(const FIntVector& WorldBlockPosition)
	{
		FIntVector2 ChunkContainingBlock(WorldBlockPosition.X / ChunkSize, WorldBlockPosition.Y / ChunkSize);
		if (WorldBlockPosition.X < 0)
		{
			ChunkContainingBlock.X -= 1;
		}
		if (WorldBlockPosition.Y < 0)
		{
			ChunkContainingBlock.Y -= 1;
		}
		return ChunkContainingBlock;
	}

	static FIntVector2 GetChunkPosition(const FVector& WorldPosition)
	{
		return GetChunkContainingBlockPosition(GetWorldBlockPosition(WorldPosition));
	}
};
