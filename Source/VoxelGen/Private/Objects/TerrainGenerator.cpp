// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/TerrainGenerator.h"
#include "FastNoiseWrapper.h"
#include "Structs/ChunkColumn.h"
#include "Structs/ChunkData.h"
#include "Engine/DataTable.h"
#include "Math/UnrealMathUtility.h"

UTerrainGenerator::UTerrainGenerator()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTerrainGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (!bIsInitialized)
	{
		WorldSize = FChunkData::GetWorldSize(this);
		if (WorldSize <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("UTerrainGenerator: Invalid World Size (%d). Check WorldSizeInChunks."), WorldSize);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("UTerrainGenerator: Initializing world data generation for size %d x %d..."), WorldSize, WorldSize);

		InitializeNoise();
		GenerateWorldData();

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("UTerrainGenerator: World data generation complete."));
	}
}


void UTerrainGenerator::InitializeNoise()
{
	SetupNoise(ContinentalnessNoise, ContinentalnessNoiseSettings);
	SetupNoise(ErosionNoise, ErosionNoiseSettings);
	SetupNoise(PeaksValleysNoise, PeaksValleysNoiseSettings);
	SetupNoise(TemperatureNoise, TemperatureNoiseSettings);
	SetupNoise(HumidityNoise, HumidityNoiseSettings);
}

void UTerrainGenerator::SetupNoise(TObjectPtr<UFastNoiseWrapper>& Noise, const UNoiseOctaveSettingsAsset* Settings)
{
	if (!Settings) {
		UE_LOG(LogTemp, Warning, TEXT("Missing Noise Settings Asset!"));
		return;
	}
	if (!Noise)
	{
		Noise = NewObject<UFastNoiseWrapper>(this);
	}
	Noise->SetupFastNoise(
		Settings->NoiseType,
		Settings->Seed,
		Settings->Frequency,
		Settings->Interpolation,
		Settings->FractalType,
		Settings->Octaves,
		Settings->Lacunarity,
		Settings->Gain
	);
}

void UTerrainGenerator::GenerateWorldData()
{
	int WorldSizeInColumns = FChunkData::GetWorldSizeInColumns(this);
	// Allocate maps
	HeightMap.Key = FMapData(WorldSizeInColumns, WorldSizeInColumns);
	TemperatureMap = FMapData(WorldSizeInColumns, WorldSizeInColumns);
	HumidityMap = FMapData(WorldSizeInColumns, WorldSizeInColumns);
	BiomeMap = FMapData(WorldSizeInColumns, WorldSizeInColumns);

	UE_LOG(LogTemp, Log, TEXT("Step 1: Generating Base Height Map..."));
	GenerateHeightMap();

	UE_LOG(LogTemp, Log, TEXT("Step 2: Simulating Climate..."));
	SimulateClimate();
}

void UTerrainGenerator::CalculateNoiseLayers(int GlobalX, int GlobalY, float& OutContinentalness, float& OutErosion,
	float& OutPeaksValleys)
{
	OutContinentalness = 0.0f;
	OutErosion = 0.0f;
	OutPeaksValleys = 0.0f;
	
	// Clamp noise values to [0, 1] range 
	float RawCont = (ContinentalnessNoise->GetNoise2D(GlobalX, GlobalY) + 1) / 2.f;
	float RawErosion = (ErosionNoise->GetNoise2D(GlobalX, GlobalY) + 1) / 2.f;
	float RawPV = (PeaksValleysNoise->GetNoise2D(GlobalX, GlobalY) + 1) / 2.f;
	
	ApplySpline(ContinentalnessSpline, RawCont, OutContinentalness);
	ApplySpline(ErosionSpline, RawErosion, OutErosion);
	ApplySpline(PeaksValleysSpline, RawPV, OutPeaksValleys);
}

void UTerrainGenerator::ApplySpline(const UCurveFloat* Spline, float InValue, float& OutValue)
{
	if (Spline)
	{
		OutValue = Spline->GetFloatValue(InValue);
	}
	else
	{
		// If no spline, pass through
		OutValue = (InValue + 1.0f) / 2.0f;
	}
}


void UTerrainGenerator::GenerateHeightMap()
{
	if (!ContinentalnessNoise || !ErosionNoise || !PeaksValleysNoise)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot generate height map, required Minecraft noises missing."));
		return;
	}
	
	int ChunkHeight = FChunkData::GetChunkHeight(this);

	int WorldSizeInColumns = FChunkData::GetWorldSizeInColumns(this);
	
	for (int y = 0; y < WorldSizeInColumns; ++y)
	{
		for (int x = 0; x < WorldSizeInColumns; ++x)
		{
			// 1. Get base noise values from noise and splines
			float Continentalness, Erosion, PeaksValleys;
			CalculateNoiseLayers(x, y, Continentalness, Erosion, PeaksValleys);

			// 2. Combine (remap to [-1, 1] style for amplitude)
			float ContRemapped = (Continentalness - 0.5f) * 2.0f;
			float ErosionRemapped = (Erosion - 0.5f) * 2.0f;
			float PVRemapped = (PeaksValleys - 0.5f) * 2.0f;
			float HeightNoise = (ContRemapped) - (ErosionRemapped) + (PVRemapped);


			// 3. Calculate absolute height
			float AbsoluteHeight = TerrainBaseHeight + HeightNoise * TerrainAmplitude;

			// 4. Clamp to world bounds and store
			int FinalBlockHeight = FMath::Clamp(FMath::RoundToInt(AbsoluteHeight), 0, ChunkHeight - 1);
			HeightMap.Key.SetData(x, y, FinalBlockHeight);
			HeightMap.Value = {Continentalness, Erosion, PeaksValleys};
		}
	}
}


void UTerrainGenerator::SimulateClimate()
{
	SimulateTemperature();
	SimulateHumidity();
}

void UTerrainGenerator::SimulateTemperature()
{
	if (!TemperatureNoise)
	{
		UE_LOG(LogTemp, Error, TEXT("TemperatureNoise not initialized!"));
		return;
	}

	int WorldSizeInColumns = FChunkData::GetWorldSizeInColumns(this);
	
	for (int y = 0; y < WorldSizeInColumns; ++y)
	{
		for (int x = 0; x < WorldSizeInColumns; ++x)
		{
			// 1. Get base temperature noise and apply spline
			float RawTemp = (TemperatureNoise->GetNoise2D(x, y) + 1) / 2.f; 

			// 2. Altitude effect: Temperature drops based on height difference from base level
			float CurrentHeight = HeightMap.Key.GetData(x, y);
			float HeightDifference = CurrentHeight - TerrainBaseHeight; // Can be negative
			float AltitudeModifier = HeightDifference * AltitudeTemperatureFactor; // Temp change due to altitude

			// 3. Combine: Start with curved base temp and subtract altitude effect
			float FinalTemp = RawTemp - AltitudeModifier;

			// Clamp final temperature to [0, 1]
			TemperatureMap.SetData(x, y, FMath::Clamp(FinalTemp, 0.0f, 1.0f));
		}
	}
}


void UTerrainGenerator::SimulateHumidity()
{
	if (!HumidityNoise)
    {
        UE_LOG(LogTemp, Error, TEXT("BaseHumidityNoise not initialized!"));
        return;
    }

	int WorldSizeInColumns = FChunkData::GetWorldSizeInColumns(this);
	// Humidity is now just the base Humidity noise, normalized.
	for (int x = 0; x < WorldSizeInColumns; ++x)
	{
		for (int y = 0; y < WorldSizeInColumns; ++y)
		{
			// 1. Get base humidity noise and apply spline
			float RawHumidity = (HumidityNoise->GetNoise2D(x, y) + 1) / 2.f;
			UE_LOG(LogTemp, Verbose, TEXT("Raw Humidity: %f"), RawHumidity);

			// 2. Store clamped humidity/precipitation
			HumidityMap.SetData(x, y, FMath::Clamp(RawHumidity, 0.0f, 1.0f));
		}
	}
}


void UTerrainGenerator::ClassifyBiomes()
{
}


// --- Column Generation ---

FChunkColumn UTerrainGenerator::GenerateColumnData(int GlobalX, int GlobalY)
{
	int ChunkHeight = FChunkData::GetChunkHeight(this);
	
	if (!bIsInitialized)
	{
		return FChunkColumn(ChunkHeight, GlobalX, GlobalY);
	}

	FChunkColumn Column(ChunkHeight, GlobalX, GlobalY);

	Column.Height = HeightMap.Key.GetData(GlobalX, GlobalY);
	
	Column.SetGenerationData(HeightMap.Value.Continentalness, HeightMap.Value.Erosion, HeightMap.Value.PeaksValleys);

	float ColumnTemperature = GetTemperatureData(GlobalX, GlobalY);
	Column.SetTemperatureData(ColumnTemperature);

	float ColumnHumidity = GetHumidityData(GlobalX, GlobalY);
	Column.SetHumidityData(ColumnHumidity);

	EBiomeType BiomeType = GetBiomeData(Column);
	Column.SetBiomeType(BiomeType);

	return Column;
}

void UTerrainGenerator::PopulateColumnBlocks(FChunkColumn& ColumnData)
{
	FBiomeSettings* BiomeSettings = GetBiomeSettings(ColumnData.GetBiomeType());
	if (!BiomeSettings)
	{
		// Handle default block assignment (e.g., stone up to height)
        for(int z = 0; z <= ColumnData.Height && z < FChunkData::GetChunkHeight(this); ++z) { ColumnData.Blocks[z] = EBlock::Stone; }
		return;
	}

	int CurrentHeight = ColumnData.Height;
	for (const auto& Layer : BiomeSettings->Layers)
	{
		if (Layer.LayerThickness <= 0) continue;
		int LayerBottomHeight = CurrentHeight - Layer.LayerThickness;
		for (int z = CurrentHeight; z >= 0 && z > LayerBottomHeight; --z)
		{
			if (z < FChunkData::GetChunkHeight(this)) { ColumnData.Blocks[z] = Layer.BlockType; }
		}
		CurrentHeight = LayerBottomHeight;
		if (CurrentHeight < 0) break;
	}

    if (CurrentHeight >= 0) {
        for (int z = CurrentHeight; z >= 0; --z) {
             if (z < FChunkData::GetChunkHeight(this)) { ColumnData.Blocks[z] = EBlock::Stone; }
        }
    }

	if (ColumnData.Height < WaterThreshold) {
		for (int z = ColumnData.Height + 1; z <= WaterThreshold; ++z) {
			if (z >= 0 && z < FChunkData::GetChunkHeight(this)) { ColumnData.Blocks[z] = EBlock::Water; }
		}
	}
}

// --- Data Accessors ---
float UTerrainGenerator::GetHeightData(int GlobalX, int GlobalY) const
{
	if (!bIsInitialized) return 0.0f;
	return HeightMap.Key.GetData(GlobalX, GlobalY);
}

float UTerrainGenerator::GetTemperatureData(int GlobalX, int GlobalY) const
{
	if (!bIsInitialized) return 0.5f;
	return TemperatureMap.GetData(GlobalX, GlobalY);
}

float UTerrainGenerator::GetHumidityData(int GlobalX, int GlobalY) const
{
	if (!bIsInitialized) return 0.5f;
	return HumidityMap.GetData(GlobalX, GlobalY);
}

EBiomeType UTerrainGenerator::GetBiomeData(const FChunkColumn& Column) const
{
	return BiomeLookupTable[static_cast<int>(Column.GetHumidityType())][static_cast<int>(Column.GetTemperatureType())];
}

// --- Helper Functions ---

bool UTerrainGenerator::IsValidCoordinate(int GlobalX, int GlobalY) const
{
	return GlobalX >= 0 && GlobalX < WorldSize && GlobalY >= 0 && GlobalY < WorldSize;
}

FBiomeSettings* UTerrainGenerator::GetBiomeSettings(EBiomeType BiomeType) const
{
	if (!BiomesTable) return nullptr;
	FName BiomeName = BiomeTypeToFName(BiomeType);
	return BiomesTable->FindRow<FBiomeSettings>(BiomeName, TEXT("Lookup Biome Settings"));
}