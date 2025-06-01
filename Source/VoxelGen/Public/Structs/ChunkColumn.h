// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TerrainData.h"
#include "VoxelGen/Enums.h"
#include "ChunkColumn.generated.h"

struct FBiomeSettings;
enum class EBlock;

USTRUCT(BlueprintType)
struct FChunkColumn
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<EBlock> Blocks;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data")
	int Height;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data")
	int X;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data")
	int Y;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	float Temperature;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	float Humidity;
	
private:
	// UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	// ETemperatureType TemperatureType;
	// UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	// EHumidityType HumidityType;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	EBiomeType BiomeType;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	float Continentalness;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	float Erosion;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk Column Data", meta = (AllowPrivateAccess = "true"))
	float PeaksValleys;
	
public:
	FChunkColumn() = default;

	FChunkColumn(int ChunkHeight, int x, int y)
		: X(x), Y(y)
	{
		Blocks.SetNum(ChunkHeight);
	}
    
	EBiomeType GetBiomeType() const { return BiomeType; }

	void SetBiomeType(EBiomeType NewBiomeType) { BiomeType = NewBiomeType;}
	
	void SetGenerationData(const FTerrainParameterData& NewTerrainData)
	{
		Continentalness = NewTerrainData.Continentalness;
		Erosion = NewTerrainData.Erosion;
		PeaksValleys = NewTerrainData.PeaksValleys;
	}
};