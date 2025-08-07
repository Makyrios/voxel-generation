// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ChunkData.generated.h"

USTRUCT()
struct FChunkData
{
	GENERATED_BODY()

public:
	static int32 GetSeed(const UObject* WorldContext);
	
	static int32 GetBlockIndex(const UObject* WorldContext, int32 X, int32 Y, int32 Z);

	static int32 GetColumnIndex(const UObject* WorldContext, int32 X, int32 Y);
	static int32 GetColumnIndexFromLocal(int32 X, int32 Y, int32 ChunkSize);
	
	static FIntVector GetLocalBlockPosition(const UObject* WorldContext, const FIntVector& WorldBlockPosition);

	static FIntVector GetWorldBlockPosition(const UObject* WorldContext, const FVector& WorldPosition);

	static FIntVector2 GetChunkContainingBlockPosition(const UObject* WorldContext, const FIntVector& WorldBlockPosition);

	static FIntVector2 GetChunkPosition(const UObject* WorldContext, const FVector& WorldPosition);

	static int32 GetChunkSize(const UObject* WorldContext);
	
	static int32 GetChunkHeight(const UObject* WorldContext);
	
	static float GetBlockSize(const UObject* WorldContext);

	static float GetBlockScale(const UObject* WorldContext);

	static float GetScaledBlockSize(const UObject* WorldContext);
};
