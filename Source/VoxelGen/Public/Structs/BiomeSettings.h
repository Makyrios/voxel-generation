// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockLayer.h"
#include "BiomeSettings.generated.h"

USTRUCT(BlueprintType)
struct VOXELGEN_API FBiomeSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	EBiomeType BiomeType = EBiomeType::Grassland;
	
	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	TArray<FBlockLayer> Layers;
	
};
