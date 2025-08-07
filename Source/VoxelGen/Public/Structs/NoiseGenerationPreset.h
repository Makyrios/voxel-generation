// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NoiseGenerationPreset.generated.h"

class UNoiseOctaveSettingsAsset;

UCLASS()
class VOXELGEN_API UNoiseGenerationPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	UNoiseOctaveSettingsAsset* Continentalness;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	UNoiseOctaveSettingsAsset* Erosion;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	UNoiseOctaveSettingsAsset* Weirdness;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	UNoiseOctaveSettingsAsset* Temperature;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	UNoiseOctaveSettingsAsset* Humidity;
	
};
