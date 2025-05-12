#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Structs/BiomeSettings.h"
#include "Structs/NoiseOctaveSettingsAsset.h"
#include "Structs/TerrainData.h" // Keep for Threshold structs
#include "VoxelGen/Enums.h"
#include "TerrainGenerator.generated.h"

// Forward Declarations
class AChunkWorld;
class UFoliageGenerator;
struct FChunkColumn;
class UFastNoiseWrapper;
class UCurveFloat; // Ensure UCurveFloat is known

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELGEN_API UTerrainGenerator : public UActorComponent
{
    GENERATED_BODY()

public:
    UTerrainGenerator();

    // THIS is now the main function. Calculates all data for a single column on demand.
    FChunkColumn GenerateColumnData(int GlobalX, int GlobalY);

    // Populates blocks based on biome and height (logic remains similar)
    void PopulateColumnBlocks(FChunkColumn& ColumnData);

    // Foliage placement logic remains similar
    void DecorateChunkWithFoliage(
        TArray<FChunkColumn>& InOutChunkColumns,
        const FIntVector2& ChunkGridPosition,
        const FRandomStream& WorldFoliageStreamBase
    ) const;

    bool IsNoiseInitialized() const { return bNoiseInitialized; }

protected:
    virtual void BeginPlay() override;

private:
    // Initializes noise generators ONCE
    void InitializeNoise();
    void SetupNoise(TObjectPtr<UFastNoiseWrapper>& Noise, const UNoiseOctaveSettingsAsset* Settings);

    // Calculates terrain parameters (Cont, Ero, Weird, PV) for a specific point
    FTerrainParameterData CalculateTerrainParameters(int GlobalX, int GlobalY) const;
    void ApplySpline(const UCurveFloat* Spline, float InValue, float& OutValue) const; // Make const

    // Determines biome type based on calculated inputs for a specific point
    EBiomeType DetermineBiomeType(const FCategorizedBiomeInputs& Params) const;

    // Biome classification helpers (remain the same, make const)
    ETemperatureType CategorizeTemperature(float TempValue_neg1_1) const;
    EHumidityType CategorizeHumidity(float HumidityValue_neg1_1) const;
    EContinentalnessType CategorizeContinentalness(float ContinentalnessValue_neg1_1) const;
    EErosionType CategorizeErosion(float ErosionValue_neg1_1) const;
    EPVType CategorizePV(float PVValue_neg1_1) const;

    // Mappers (remain the same, make const)
    EBiomeType MapMiddleBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness) const;
    EBiomeType MapPlateauBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness) const;
    EBiomeType MapBeachBiome(ETemperatureType Temp) const;
    EBiomeType MapShatteredBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness) const;

    // Helper to get biome settings from table (make const)
    FBiomeSettings* GetBiomeSettings(EBiomeType BiomeType) const;

    // Helper for remapping (make const)
    float Remap01toNeg11(float Value01) const { return Value01 * 2.0f - 1.0f; }


public:
	UPROPERTY(EditAnywhere, Category = "Settings|Biomes")
	TObjectPtr<UDataTable> BiomesTable;

	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> ContinentalnessNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> ErosionNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> WeirdnessNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> TemperatureNoiseSettings;
	UPROPERTY(EditAnywhere, Category = "Settings|Noise")
	TObjectPtr<UNoiseOctaveSettingsAsset> HumidityNoiseSettings;

	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	int32 WaterThreshold = 55;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	float AltitudeTemperatureFactor = 0.01f;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	float TerrainBaseHeight = 60.0f;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation")
	float TerrainAmplitude = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> ContinentalnessSpline;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> ErosionSpline;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> WeirdnessSpline;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	TObjectPtr<UCurveFloat> PeaksValleysSpline;

	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	float ContinentalnessWeight = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	float ErosionWeight = 0.2f; // This was unused in Height Calc? Add if needed.
	UPROPERTY(EditAnywhere, Category = "Settings|Splines")
	float PeaksValleysWeight = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation|Thresholds")
	FTemperatureData TemperatureThresholds;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation|Thresholds")
	FHumidityData HumidityThresholds;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation|Thresholds")
	FContinentalnessData ContinentalnessThresholds;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation|Thresholds")
	FErosionData ErosionThresholds;
	UPROPERTY(EditAnywhere, Category = "Settings|Terrain Generation|Thresholds")
	FPeaksValleysData PeaksValleysThresholds;

private:
    UPROPERTY()
	TObjectPtr<UFoliageGenerator> FoliageGenerator;

	UPROPERTY()
	TObjectPtr<AChunkWorld> ParentWorld;

	UPROPERTY() TObjectPtr<UFastNoiseWrapper> ContinentalnessNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> ErosionNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> WeirdnessNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> TemperatureNoise;
	UPROPERTY() TObjectPtr<UFastNoiseWrapper> HumidityNoise;

	// Internal State
	bool bNoiseInitialized = false;
};