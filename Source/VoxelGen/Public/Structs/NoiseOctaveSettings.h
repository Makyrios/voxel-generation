// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FastNoiseWrapper.h"
#include "VoxelGen/Enums.h"
#include "NoiseOctaveSettings.generated.h"

enum class EFastNoise_NoiseType : uint8;

USTRUCT()
struct VOXELGEN_API FNoiseOctaveSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	EFastNoise_NoiseType NoiseType = EFastNoise_NoiseType::Perlin;
	UPROPERTY(EditAnywhere)
	int Seed = 1337;
	UPROPERTY(EditAnywhere)
	float Frequency = 0.1f;
	UPROPERTY(EditAnywhere)
	float Amplitude = 1.0f;
};
