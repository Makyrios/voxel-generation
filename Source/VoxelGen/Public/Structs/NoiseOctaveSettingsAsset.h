// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FastNoiseWrapper.h"
#include "NoiseOctaveSettingsAsset.generated.h"

enum class EFastNoise_NoiseType : uint8;

UCLASS()
class VOXELGEN_API UNoiseOctaveSettingsAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	EFastNoise_NoiseType NoiseType = EFastNoise_NoiseType::Perlin;
	UPROPERTY(EditAnywhere)
	int SeedOffset = 1337;
	UPROPERTY(EditAnywhere)
	float Frequency = 0.1f;
	UPROPERTY(EditAnywhere)
	float Amplitude = 1.0f;
	UPROPERTY(EditAnywhere)
	int Octaves = 3;
	UPROPERTY(EditAnywhere)
	float Lacunarity = 2.0f;
	UPROPERTY(EditAnywhere)
	float Gain = 0.5f;
	UPROPERTY(EditAnywhere)
	EFastNoise_Interp Interpolation = EFastNoise_Interp::Quintic;
	UPROPERTY(EditAnywhere)
	EFastNoise_FractalType FractalType = EFastNoise_FractalType::FBM;
	
};
