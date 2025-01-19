// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockLayer.h"
#include "NoiseOctaveSettings.h"
#include "BiomeSettingsStruct.generated.h"

USTRUCT(BlueprintType)
struct VOXELGEN_API FBiomeSettingsStruct : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	FName BiomeName;

	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	int BaseHeight = 8;

	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	TArray<FNoiseOctaveSettings> OctaveSettings;
	
	UPROPERTY(EditAnywhere, Category = "Biome Settings")
	TArray<FBlockLayer> Layers;
	
};
