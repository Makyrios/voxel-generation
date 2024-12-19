// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ChunkData.generated.h"

USTRUCT()
struct FChunkData
{
	GENERATED_BODY()

	static constexpr int32 ChunkSize = 16;
	static constexpr float BlockSize = 100.f;
	static constexpr float BlockScale = 0.5f;
	static constexpr int32 WorldSizeInChunks = 20;
};
