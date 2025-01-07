// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Structs/NoiseOctaveSettings.h"
#include "BiomeSettingsAsset.generated.h"

UCLASS()
class VOXELGEN_API UBiomeSettingsAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Settings")
    int BaseHeight = 8;

    UPROPERTY(EditAnywhere, Category = "Biome Settings")
    TArray<FNoiseOctaveSettings> OctaveSettings;
	
};
