// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WorldGameInstance.generated.h"

UCLASS()
class VOXELGEN_API UWorldGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 WorldSize = 32;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 ChunkSize = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 ChunkHeight = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	float BlockSize = 100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	float BlockScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
	int32 WaterThreshold = 10;
};
