// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockLayer.h"
#include "BiomeSettings.generated.h"

USTRUCT(BlueprintType)
struct FFoliageSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	EFoliageType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChancePerColumn = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	int32 MinHeight = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	int32 MaxHeight = 7;
    
	// Chance for a "large" or variant version
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VariantChance = 0.1f; 

	// List of valid ground blocks this foliage can spawn on
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	TArray<EBlock> AllowedSurfaceBlocks; 
};

USTRUCT(BlueprintType)
struct VOXELGEN_API FBiomeSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	EBiomeType BiomeType = EBiomeType::Grassland;
	
	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	TArray<FBlockLayer> Layers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	TArray<FFoliageSpawnRule> FoliageRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SurfaceGrassChance = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	TArray<EBlock> GrassSpawnableOn;
};
