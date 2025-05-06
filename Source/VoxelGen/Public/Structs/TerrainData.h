// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChunkData.h"
#include "VoxelGen/Enums.h"
#include "TerrainData.generated.h"

USTRUCT(BlueprintType)
struct FBiomeWeight
{
	GENERATED_BODY()

	UPROPERTY()
	EBiomeType Biome;

	UPROPERTY()
	float Weight;

	FBiomeWeight() : Biome(EBiomeType::Grassland), Weight(0.0f) {}
	FBiomeWeight(EBiomeType InBiome, float InWeight) : Biome(InBiome), Weight(InWeight) {}
};

USTRUCT()
struct FHeightData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float DeepWater = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ShallowWater = 0.4f;	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Sand = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Grass = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Forest = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Rock = 0.9f;
};

USTRUCT()
struct FHeatData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ColdestValue = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ColderValue = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ColdValue = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WarmValue = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WarmerValue = 0.8f;
};

USTRUCT()
struct FHumidityData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float DryerValue = 0.27f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float DryValue = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WetValue = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WetterValue = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WettestValue = 0.9f;
};

USTRUCT()
struct FTerrainNoises
{
	GENERATED_BODY()

	float Continentalness;
	float Erosion;
	float PeaksValleys;

	FTerrainNoises() = default;

	FTerrainNoises(float InContinentalness, float InErosion, float InPeaksValleys)
		: Continentalness(InContinentalness), Erosion(InErosion), PeaksValleys(InPeaksValleys) {}
};

struct FMapData
{
	TArray<TArray<float>> Data;
	int Width;
	int Length;

	FMapData() = default;

	FMapData(int width, int length) : Width(width), Length(length)
	{
		Data.SetNum(width);
		for (int i = 0; i < width; ++i)
		{
			Data[i].SetNum(length);
		}
	}

	float GetData(int x, int y) const
	{
		return Data[x][y];
	}

	void SetData(int x, int y, float Value)
	{
		Data[x][y] = Value;
	}
	
	// Normalize data map to [0, 1] range
	void NormalizeData()
	{
		float MinVal, MaxVal;
		GetDataMinMax(MinVal, MaxVal);
		const float Range = MaxVal - MinVal;
		if (Width == 0 || Length == 0 || FMath::IsNearlyZero(Range)) return;

		for (int x = 0; x < Width; ++x)
		{
			for (int y = 0; y < Length; ++y)
			{
				Data[x][y] = (Data[x][y] - MinVal) / Range;
			}
		}
	}

	void GetDataMinMax(float& OutMin, float& OutMax) const
	{
		OutMin = TNumericLimits<float>::Max();
		OutMax = TNumericLimits<float>::Min();
		if (Width == 0 || Length == 0) return;

		for (int x = 0; x < Width; ++x)
		{
			for (int y = 0; y < Length; ++y)
			{
				const float Val = Data[x][y];
				if (Val < OutMin) OutMin = Val;
				if (Val > OutMax) OutMax = Val;
			}
		}
	}
};