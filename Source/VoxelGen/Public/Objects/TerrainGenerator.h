// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Structs/BiomeSettings.h"
#include "Structs/NoiseOctaveSettingsAsset.h"
#include "Structs/TerrainData.h"
#include "VoxelGen/Enums.h"
#include "TerrainGenerator.generated.h"

struct FChunkColumn;
class UFastNoiseWrapper;

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELGEN_API UTerrainGenerator : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerrainGenerator();

	FChunkColumn GenerateColumnData(int GlobalX, int GlobalY);
	void PopulateColumnBlocks(FChunkColumn& ColumnData);

	float GetHeightData(int GlobalX, int GlobalY) const;
	float GetTemperatureData(int GlobalX, int GlobalY) const;
	float GetHumidityData(int GlobalX, int GlobalY) const;
	EBiomeType GetBiomeData(const FChunkColumn& Column) const;

	// Added getter for initialization status
	bool IsInitialized() const { return bIsInitialized; }
    int GetWorldSize() const { return WorldSize; } // Getter for world size

protected:
	virtual void BeginPlay() override;

private:
	// World generation pipeline steps
	void InitializeNoise();
	void SetupNoise(TObjectPtr<UFastNoiseWrapper>&, const UNoiseOctaveSettingsAsset* Settings);
	void GenerateWorldData();
	void CalculateNoiseLayers(int GlobalX, int GlobalY, float& OutContinentalness, float& OutErosion, float& OutPeaksValleys);
	void ApplySpline(const UCurveFloat* Spline, float InValue, float& OutValue);
	void GenerateHeightMap();
	void SimulateClimate();
	void SimulateTemperature();
	void SimulateHumidity();
	void ClassifyBiomes();

	FBiomeSettings* GetBiomeSettings(EBiomeType BiomeType) const;
	bool IsValidCoordinate(int GlobalX, int GlobalY) const;

public:
	UPROPERTY(EditAnywhere, Category = "Settings|Biomes")
	TObjectPtr<UDataTable> BiomesTable;
	
	// Noise settings
	
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> ContinentalnessNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> ErosionNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> PeaksValleysNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> TemperatureNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> HumidityNoiseSettings;

	// Thresholds and parameters
	
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	FHeightData HeightThresholds;
	
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	int32 WaterThreshold = 55;

	// How much temp drops with height [0, 1]
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	float AltitudeTemperatureFactor = 0.01f;

	// Base height of the terrain
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	float TerrainBaseHeight = 60.0f;

	// Max deviation from BaseHeight due to noise
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	float TerrainAmplitude = 50.0f;

	// Splines
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> ContinentalnessSpline;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> ErosionSpline;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> PeaksValleysSpline;
	

	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	float ContinentalnessWeight = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	float ErosionWeight = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	float PeaksValleysWeight = 0.3f;
	
private:
	// Noise instances
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> ContinentalnessNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> ErosionNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> PeaksValleysNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> TemperatureNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> HumidityNoise;

	// Precalculated World Data Maps
	TPair<FMapData, FTerrainNoises> HeightMap;
	FMapData TemperatureMap;
	FMapData HumidityMap;
	FMapData BiomeMap;
	
	// Internal State
	int WorldSize = 0;
	bool bIsInitialized = false;

	// Biome lookup table (Whittaker diagram inspired)
	const TArray<TArray<EBiomeType>> BiomeLookupTable = {
		// Coldest    Colder      Cold        Warm         Warmer       Warmest
		{ EBiomeType::Ice, EBiomeType::Tundra, EBiomeType::Grassland, EBiomeType::Desert, EBiomeType::Desert, EBiomeType::Desert },         // Dryest
		{ EBiomeType::Ice, EBiomeType::Tundra, EBiomeType::Grassland, EBiomeType::Desert, EBiomeType::Desert, EBiomeType::Desert },         // Dryer
		{ EBiomeType::Ice, EBiomeType::Tundra, EBiomeType::Woodland, EBiomeType::Woodland, EBiomeType::Savanna, EBiomeType::Savanna },      // Dry
		{ EBiomeType::Ice, EBiomeType::Tundra, EBiomeType::BorealForest, EBiomeType::Woodland, EBiomeType::Savanna, EBiomeType::Savanna },  // Wet
		{ EBiomeType::Ice, EBiomeType::Tundra, EBiomeType::BorealForest, EBiomeType::SeasonalForest, EBiomeType::TropicalRainforest, EBiomeType::TropicalRainforest }, // Wetter
		{ EBiomeType::Ice, EBiomeType::Tundra, EBiomeType::BorealForest, EBiomeType::TemperateRainforest, EBiomeType::TropicalRainforest, EBiomeType::TropicalRainforest } // Wettest
	};
};