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
struct FTemperatureData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ColdestValue = -0.45f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ColderValue = -0.15f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float TemperateValue = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WarmValue = 0.55f;
};

USTRUCT()
struct FHumidityData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float DryestValue = -0.35f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float DryValue = -0.1f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float MediumValue = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float WetValue = 0.3f;
};

USTRUCT()
struct FContinentalnessData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float MushroomFieldsValue = -0.8f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float DeepOceanValue = -0.45;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float OceanValue = -0.13f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float CoastValue = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float NearInlandValue = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float MidInlandValue = 0.8f;
};

USTRUCT()
struct FErosionData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float E0Value = -0.78f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float E1Value = -0.375f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float E2Value = -0.2225f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float E3Value = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float E4Value = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float E5Value = 0.65f;
};

USTRUCT()
struct FPeaksValleysData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ValleysValue = -0.85f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float LowValue = -0.6f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float MidValue = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Settings")
	float HighValue = 0.7f;
};

USTRUCT()
struct FTerrainParameterData
{
	GENERATED_BODY()

	float Continentalness;
	float Erosion;
	float Weirdness;
	float PeaksValleys;

	FTerrainParameterData() = default;

	FTerrainParameterData(float InContinentalness, float InErosion, float InWeirdness, float InPeaksValleys)
		: Continentalness(InContinentalness), Erosion(InErosion), Weirdness(InWeirdness), PeaksValleys(InPeaksValleys) {}
};

USTRUCT()
struct FCategorizedBiomeInputs
{
	GENERATED_BODY()
	
	ETemperatureType Temperature;
	EHumidityType Humidity;
	EContinentalnessType Continentalness;
	EErosionType Erosion;
	EPVType PV;
	float WeirdnessValue; // Raw weirdness value [-1, 1], for checks like W > 0

	FCategorizedBiomeInputs(
		ETemperatureType T, EHumidityType H, EContinentalnessType C,
		EErosionType E, EPVType P, float W)
		: Temperature(T), Humidity(H), Continentalness(C), Erosion(E), PV(P), WeirdnessValue(W) {}

	FCategorizedBiomeInputs() = default;
};


template<typename T>
struct FMapData
{
	TArray<TArray<T>> Data;
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

	T GetData(int x, int y) const
	{
		return Data[x][y];
	}

	void SetData(int x, int y, T Value)
	{
		Data[x][y] = Value;
	}
};