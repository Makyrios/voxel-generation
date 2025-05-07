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
		WorldSizeInColumns = FChunkData::GetWorldSizeInColumns(this);
		if (WorldSizeInColumns <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("UTerrainGenerator: Invalid World Size (%d). Check WorldSizeInChunks."), WorldSizeInColumns);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("UTerrainGenerator: Initializing world data generation for size %d x %d..."), WorldSizeInColumns, WorldSizeInColumns);

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
	SetupNoise(WeirdnessNoise, WeirdnessNoiseSettings);
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
	// Allocate maps
	HeightMap = FMapData<float>(WorldSizeInColumns, WorldSizeInColumns);
	TemperatureMap = FMapData<float>(WorldSizeInColumns, WorldSizeInColumns);
	TerrainParamsMap = FMapData<FTerrainParameterData>(WorldSizeInColumns, WorldSizeInColumns);
	HumidityMap = FMapData<float>(WorldSizeInColumns, WorldSizeInColumns);
	BiomeIDMap = FMapData<float>(WorldSizeInColumns, WorldSizeInColumns);

	UE_LOG(LogTemp, Log, TEXT("Step 1: Generating Base Height Map..."));
	GenerateHeightMap();

	UE_LOG(LogTemp, Log, TEXT("Step 2: Simulating Climate..."));
	SimulateClimate();

	UE_LOG(LogTemp, Log, TEXT("Step 3: Classifying Biomes..."));
	ClassifyBiomes();
}

void UTerrainGenerator::CalculateTerrainParameters(int GlobalX, int GlobalY, float& OutContinentalness, float& OutErosion,
	float& OutSplinedWeirdness, float& OutPeaksValleys)
{
	float RawCont = ContinentalnessNoise->GetNoise2D(GlobalX, GlobalY);
	float RawErosion = ErosionNoise->GetNoise2D(GlobalX, GlobalY);
	float RawWeirdness = WeirdnessNoise->GetNoise2D(GlobalX, GlobalY);

	// Normalize to [0, 1]
	float Cont = (RawCont + 1) / 2.f;
	float Ero = (RawErosion + 1) / 2.f;
	float Weird = (RawWeirdness + 1) / 2.f;
	
	ApplySpline(ContinentalnessSpline, Cont, OutContinentalness);
	ApplySpline(ErosionSpline, Ero, OutErosion);
	ApplySpline(WeirdnessSpline, Weird, OutSplinedWeirdness);
	
	// Calculate PV from RAW Weirdness [-1, 1]
	// PV = 1 - |(3 * |weirdness|) - 2|
	float PV_neg1_1 = 1.0f - FMath::Abs((3.0f * FMath::Abs(RawWeirdness)) - 2.0f);
    
	// Normalize PV to [0, 1] for spline input
	float PV01_forSpline = (PV_neg1_1 + 1.f) / 2.f;
	ApplySpline(PeaksValleysSpline, PV01_forSpline, OutPeaksValleys);
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
	if (!ContinentalnessNoise || !ErosionNoise || !WeirdnessNoise)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot generate height map, required Minecraft noises missing."));
		return;
	}
	
	int ChunkHeight = FChunkData::GetChunkHeight(this);
	
	for (int y = 0; y < WorldSizeInColumns; ++y)
	{
		for (int x = 0; x < WorldSizeInColumns; ++x)
		{
			// 1. Get base noise values from noise and splines
			float Continentalness, Erosion, Weirdness, PeaksValleys;
			CalculateTerrainParameters(x, y, Continentalness, Erosion, Weirdness, PeaksValleys);

			TerrainParamsMap.SetData(x,y, FTerrainParameterData(Continentalness, Erosion, Weirdness, PeaksValleys));

			// Height Calculation (using splined [0,1] values, remapped to [-1,1] style for amplitude)
            // The weights might need adjustment or a different combination formula
			float ContRemapped = Remap01toNeg11(Continentalness);
			float ErosionRemapped = Remap01toNeg11(Erosion);
			float PVRemapped = Remap01toNeg11(PeaksValleys);
            
            // Example combination: C influences base, E flattens, PV adds features
            // This is highly tunable. The provided theory doesn't specify how these combine for height.
            // Original: float HeightNoise = (ContRemapped) - (ErosionRemapped) + (PVRemapped);
            // Using weights:
            float HeightNoise = (ContRemapped * ContinentalnessWeight) - 
                                (ErosionRemapped * ErosionWeight) + 
                                (PVRemapped * PeaksValleysWeight);
            
            // Normalize weighted sum if weights don't sum to 1, or ensure they do.
            // For simplicity, let's assume weights are balanced or the amplitude handles scaling.

            float AbsoluteHeight = TerrainBaseHeight + HeightNoise * TerrainAmplitude;
            int FinalBlockHeight = FMath::Clamp(FMath::RoundToInt(AbsoluteHeight), 0, ChunkHeight - 1);
            HeightMap.SetData(x, y, FinalBlockHeight);
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
	
	for (int y = 0; y < WorldSizeInColumns; ++y)
	{
		for (int x = 0; x < WorldSizeInColumns; ++x)
		{
			// 1. Get base temperature noise
			float RawTemp = TemperatureNoise->GetNoise2D(x, y);
			float Temp01 = (RawTemp + 1) / 2.f; // Normalize to [0, 1]

			// 2. Altitude effect: Temperature drops based on height difference from base level
			float CurrentHeight = HeightMap.GetData(x, y);
			float HeightDifference = CurrentHeight - TerrainBaseHeight; // Can be negative
			float AltitudeModifier = HeightDifference * AltitudeTemperatureFactor; // Temp change due to altitude

			// 3. Combine: Start with curved base temp and subtract altitude effect
			float FinalTemp = Temp01 - AltitudeModifier;

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
	
	// Humidity is now just the base Humidity noise, normalized.
	for (int x = 0; x < WorldSizeInColumns; ++x)
	{
		for (int y = 0; y < WorldSizeInColumns; ++y)
		{
			// 1. Get base humidity noise and apply spline
			float RawHumidity = HumidityNoise->GetNoise2D(x, y);
			float Humidity01 = (RawHumidity + 1) / 2.f; // [0, 1]

			// 2. Store clamped humidity/precipitation
			HumidityMap.SetData(x, y, FMath::Clamp(Humidity01, 0.0f, 1.0f));
		}
	}
}


void UTerrainGenerator::ClassifyBiomes()
{
    for (int y = 0; y < WorldSizeInColumns; ++y)
    {
        for (int x = 0; x < WorldSizeInColumns; ++x)
        {
            // Get splined [0,1] (or spline output range) parameters
            const FTerrainParameterData& Params01 = TerrainParamsMap.GetData(x, y);
            float Temp01 = TemperatureMap.GetData(x, y);
            float Humid01 = HumidityMap.GetData(x, y);

            // Remap to [-1, 1] for categorization based on theory's ranges
            float Cont_neg1_1 = Remap01toNeg11(Params01.Continentalness);
            float Ero_neg1_1 = Remap01toNeg11(Params01.Erosion);
            float Weird_neg1_1 = Remap01toNeg11(Params01.Weirdness);
        	
            float PV_neg1_1 = Remap01toNeg11(Params01.PeaksValleys);
            float Temp_neg1_1 = Remap01toNeg11(Temp01);
            float Humid_neg1_1 = Remap01toNeg11(Humid01);
            
            // Categorize
            ETemperatureType CatTemp = CategorizeTemperature(Temp_neg1_1);
            EHumidityType CatHumid = CategorizeHumidity(Humid_neg1_1);
            EContinentalnessType CatCont = CategorizeContinentalness(Cont_neg1_1);
            EErosionType CatEro = CategorizeErosion(Ero_neg1_1);
            EPVType CatPV = CategorizePV(PV_neg1_1);

            FCategorizedBiomeInputs BiomeInputs(CatTemp, CatHumid, CatCont, CatEro, CatPV, Weird_neg1_1);
            
            EBiomeType FinalBiome = DetermineBiomeType(BiomeInputs);
            BiomeIDMap.SetData(x, y, static_cast<float>(FinalBiome));
        }
    }
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

	Column.Height = HeightMap.GetData(GlobalX, GlobalY);
	
	Column.SetGenerationData(TerrainParamsMap.GetData(GlobalX, GlobalY));

	float ColumnTemperature = GetTemperatureData(GlobalX, GlobalY);
	float ColumnHumidity = GetHumidityData(GlobalX, GlobalY);
	Column.SetClimateData(ColumnTemperature, ColumnHumidity);

	EBiomeType BiomeType = GetBiomeTypeFromMap(GlobalX, GlobalY);
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

EBiomeType UTerrainGenerator::DetermineBiomeType(const FCategorizedBiomeInputs& Params) const
{
	// Stage 1: Non-Inland Biomes (Oceans, Mushroom Fields)
    // These are not based on humidity, erosion, weirdness
    if (Params.Continentalness == EContinentalnessType::MushroomFields)
    {
        return EBiomeType::Woodland; // Closest available for "Mushroom"
    }
    if (Params.Continentalness == EContinentalnessType::DeepOcean ||
        Params.Continentalness == EContinentalnessType::Ocean)
    {
        switch (Params.Temperature)
        {
            case ETemperatureType::Coldest: // Frozen Ocean
                return EBiomeType::Ice;
            case ETemperatureType::Cold:    // Cold Ocean
                return EBiomeType::Tundra; // Or Ice if you want more frozen water
            // Lukewarm, Warm, Regular Ocean
            case ETemperatureType::Temperate:
            case ETemperatureType::Warm:
            case ETemperatureType::Hot:
                 // Rely on WaterThreshold to make these watery.
                 // Grassland is a neutral placeholder. Desert if you want barren ocean floor.
                return EBiomeType::Grassland; 
            default: return EBiomeType::Grassland;
        }
    }

    // Stage 2: Inland Biomes (Coast, Near-Inland, Mid-Inland, Far-Inland)
    // This is where the big table from the theory comes in.
    // This is a VASTLY simplified version. A full implementation would be hundreds of lines.

    // Simplified: River-like biomes (Valleys PV)
    if (Params.PV == EPVType::Valleys) {
        if (Params.Temperature == ETemperatureType::Coldest) return EBiomeType::Ice; // Frozen River
        // For other rivers, could be Grassland and carved lower, or rely on WaterThreshold
        // If E6 (high erosion, flat), it could be swampy
        if (Params.Erosion == EErosionType::Level6) {
            if (Params.Temperature == ETemperatureType::Cold || Params.Temperature == ETemperatureType::Temperate)
                return EBiomeType::SeasonalForest; // Swamp -> SeasonalForest or TemperateRainforest
            if (Params.Temperature == ETemperatureType::Warm || Params.Temperature == ETemperatureType::Hot)
                return EBiomeType::TropicalRainforest; // Mangrove Swamp -> TropicalRainforest
        }
        return EBiomeType::Grassland; // Generic River
    }

    // Simplified: Peak-like biomes
    if (Params.PV == EPVType::Peaks || Params.PV == EPVType::High) {
        if (Params.Temperature == ETemperatureType::Coldest || Params.Temperature == ETemperatureType::Cold || Params.Temperature == ETemperatureType::Temperate)
            return EBiomeType::Ice; // Jagged/Frozen Peaks
        if (Params.Temperature == ETemperatureType::Warm)
            return EBiomeType::Tundra; // Stony Peaks -> Tundra as high altitude rock
        if (Params.Temperature == ETemperatureType::Hot && Params.Erosion <= EErosionType::Level1) // Badlands Peaks
             return EBiomeType::Desert;
    }
    
    // Simplified: Beach-like (Coast, Low PV, certain Erosions)
    if (Params.Continentalness == EContinentalnessType::Coast && Params.PV == EPVType::Low) {
         return MapBeachBiome(Params.Temperature);
    }


    // Catch-all for "Middle Biomes" / "Plateau Biomes" etc. using a simpler Whittaker-style lookup
    // This replaces your original BiomeLookupTable with a code version,
    // but now it's after more complex rules have been (partially) applied.
    // This is where you'd put the detailed "Middle Biomes", "Plateau Biomes" logic if expanding.
    // For now, using the provided mapping functions which are simplified.
    // Example: Check for conditions that would lead to Badlands
    bool IsBadlandsCondition = (Params.Temperature == ETemperatureType::Hot &&
                               (Params.Continentalness == EContinentalnessType::MidInland || Params.Continentalness == EContinentalnessType::FarInland) &&
                               (Params.PV == EPVType::Valleys || Params.PV == EPVType::Low || (Params.PV == EPVType::Mid && Params.Erosion <= EErosionType::Level2))
                               ) ||
                               (Params.PV == EPVType::High && Params.Erosion == EErosionType::Level0 && Params.Temperature == ETemperatureType::Hot) ||
                               (Params.PV == EPVType::Peaks && Params.Erosion == EErosionType::Level0 && Params.Temperature == ETemperatureType::Hot);

    if(IsBadlandsCondition) return MapBadlandsBiome(Params.Humidity, Params.WeirdnessValue);


    // Default to a "Middle Biome" style mapping if nothing else fits
    return MapMiddleBiome(Params.Temperature, Params.Humidity, Params.WeirdnessValue);
}

ETemperatureType UTerrainGenerator::CategorizeTemperature(float TempValue_neg1_1) const
{
	if (TempValue_neg1_1 < TemperatureThresholds.ColdestValue) return ETemperatureType::Coldest;
	if (TempValue_neg1_1 < TemperatureThresholds.ColderValue) return ETemperatureType::Cold;
	if (TempValue_neg1_1 < TemperatureThresholds.TemperateValue) return ETemperatureType::Temperate;
	if (TempValue_neg1_1 < TemperatureThresholds.WarmValue) return ETemperatureType::Warm;
	return ETemperatureType::Hot;
}

EHumidityType UTerrainGenerator::CategorizeHumidity(float HumidityValue_neg1_1) const
{
	if (HumidityValue_neg1_1 < HumidityThresholds.DryestValue) return EHumidityType::Dryest;
	if (HumidityValue_neg1_1 < HumidityThresholds.DryValue) return EHumidityType::Dry;
	if (HumidityValue_neg1_1 < HumidityThresholds.MediumValue) return EHumidityType::Medium;
	if (HumidityValue_neg1_1 < HumidityThresholds.WetValue) return EHumidityType::Wet;
	return EHumidityType::Wettest;
}

EContinentalnessType UTerrainGenerator::CategorizeContinentalness(float ContinentalnessValue_neg1_1) const
{
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.MushroomFieldsValue) return EContinentalnessType::MushroomFields;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.DeepOceanValue) return EContinentalnessType::DeepOcean;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.OceanValue) return EContinentalnessType::Ocean;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.CoastValue) return EContinentalnessType::Coast;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.NearInlandValue) return EContinentalnessType::NearInland;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.MidInlandValue) return EContinentalnessType::MidInland;
	return EContinentalnessType::FarInland;
}

EErosionType UTerrainGenerator::CategorizeErosion(float ErosionValue_neg1_1) const
{
	if (ErosionValue_neg1_1 < ErosionThresholds.E0Value) return EErosionType::Level0;
	if (ErosionValue_neg1_1 < ErosionThresholds.E1Value) return EErosionType::Level1;
	if (ErosionValue_neg1_1 < ErosionThresholds.E2Value) return EErosionType::Level2;
	if (ErosionValue_neg1_1 < ErosionThresholds.E3Value) return EErosionType::Level3;
	if (ErosionValue_neg1_1 < ErosionThresholds.E4Value) return EErosionType::Level4;
	if (ErosionValue_neg1_1 < ErosionThresholds.E5Value) return EErosionType::Level5;
	return EErosionType::Level6;
}

EPVType UTerrainGenerator::CategorizePV(float PVValue_neg1_1) const
{
	if (PVValue_neg1_1 < PeaksValleysThresholds.ValleysValue) return EPVType::Valleys;
	if (PVValue_neg1_1 < PeaksValleysThresholds.LowValue) return EPVType::Low;
	if (PVValue_neg1_1 < PeaksValleysThresholds.MidValue) return EPVType::Mid;
	if (PVValue_neg1_1 < PeaksValleysThresholds.HighValue) return EPVType::High;
	return EPVType::Peaks;
}

EBiomeType UTerrainGenerator::MapMiddleBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const
{
    // This is a direct translation of the "Middle Biomes" table, simplified to your EBiomeType
    // T=0 (Coldest), T=1 (Cold), T=2 (Temperate), T=3 (Warm), T=4 (Hot)
    // H=0 (Dryest), H=1 (Dry), H=2 (Medium), H=3 (Wet), H=4 (Wettest)

    switch (Temp)
    {
    case ETemperatureType::Coldest: // T=0
        switch (Hum)
        {
        case EHumidityType::Dryest: // Snowy Plains / Ice Spikes
        case EHumidityType::Dry:    // Snowy Plains
        case EHumidityType::Medium: // Snowy Plains / Snowy Taiga
            return Weirdness_neg1_1 > 0 ? EBiomeType::Ice : EBiomeType::Tundra; // Ice Spikes variant to Ice
        case EHumidityType::Wet:    // Snowy Taiga
        case EHumidityType::Wettest:// Taiga
            return EBiomeType::BorealForest;
        default: return EBiomeType::Tundra;
        }
    case ETemperatureType::Cold: // T=1
        switch (Hum)
        {
        case EHumidityType::Dryest: // Plains
        case EHumidityType::Dry:    // Plains
            return EBiomeType::Grassland;
        case EHumidityType::Medium: // Forest
            return EBiomeType::Woodland;
        case EHumidityType::Wet:    // Taiga
            return EBiomeType::BorealForest;
        case EHumidityType::Wettest:// Old Growth Taiga
            return EBiomeType::BorealForest; // Could be denser/different variant
        default: return EBiomeType::Grassland;
        }
    case ETemperatureType::Temperate: // T=2
        switch (Hum)
        {
        case EHumidityType::Dryest: // Flower Forest / Sunflower Plains
            return Weirdness_neg1_1 > 0 ? EBiomeType::Grassland : EBiomeType::SeasonalForest; // Sunflower Plains to Grassland
        case EHumidityType::Dry:    // Plains
             return EBiomeType::Grassland;
        case EHumidityType::Medium: // Forest / Plains
            return Weirdness_neg1_1 > 0 ? EBiomeType::Grassland : EBiomeType::Woodland;
        case EHumidityType::Wet:    // Birch Forest
            return EBiomeType::SeasonalForest;
        case EHumidityType::Wettest:// Dark Forest
            return EBiomeType::TemperateRainforest; // Dark forest is dense and often moist
        default: return EBiomeType::Woodland;
        }
    case ETemperatureType::Warm: // T=3
        switch (Hum)
        {
        case EHumidityType::Dryest: // Savanna
        case EHumidityType::Dry:    // Savanna
            return EBiomeType::Savanna;
        case EHumidityType::Medium: // Savanna
             return EBiomeType::Savanna;
        case EHumidityType::Wet:    // Jungle / Sparse Jungle
        case EHumidityType::Wettest:// Jungle / Bamboo Jungle
            return EBiomeType::TropicalRainforest;
        default: return EBiomeType::Savanna;
        }
    case ETemperatureType::Hot: // T=4
        // Most Hum levels result in Desert for T=4 in Middle Biomes table
        return EBiomeType::Desert;
    default:
        return EBiomeType::Grassland; // Fallback
    }
}

EBiomeType UTerrainGenerator::MapPlateauBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const {
    // Similar to MiddleBiomes, but some specific changes (e.g., Meadow, Savanna Plateau)
    // This is a simplified stub. For full detail, expand like MapMiddleBiome.
    // For now, it can often default to middle biome logic or specific overrides.
    if (Temp == ETemperatureType::Warm && (Hum == EHumidityType::Dryest || Hum == EHumidityType::Dry)) {
        return EBiomeType::Savanna; // Savanna Plateau
    }
    if (Temp == ETemperatureType::Hot) { // Badlands variants on plateaus
        return MapBadlandsBiome(Hum, Weirdness_neg1_1);
    }
    // Fallback to middle biome logic for other cases for simplicity here
    return MapMiddleBiome(Temp, Hum, Weirdness_neg1_1);
}

EBiomeType UTerrainGenerator::MapBeachBiome(ETemperatureType Temp) const {
    // T=0 Snowy Beach
    // T=1,2,3 Beach
    // T=4 Desert (as beach)
    switch (Temp) {
        case ETemperatureType::Coldest: return EBiomeType::Tundra; // Snowy Beach
        case ETemperatureType::Cold:
        case ETemperatureType::Temperate:
        case ETemperatureType::Warm: return EBiomeType::Grassland; // Beach (sand, use Grassland as placeholder)
        case ETemperatureType::Hot: return EBiomeType::Desert;   // Desert Beach
        default: return EBiomeType::Grassland;
    }
}

EBiomeType UTerrainGenerator::MapBadlandsBiome(EHumidityType Hum, float Weirdness_neg1_1) const {
    // H=0,1 Badlands (W<0) / Eroded Badlands (W>0)
    // H=2 Badlands
    // H=3,4 Wooded Badlands
    switch (Hum) {
        case EHumidityType::Dryest:
        case EHumidityType::Dry:
        case EHumidityType::Medium:
            return EBiomeType::Desert; // Badlands / Eroded Badlands
        case EHumidityType::Wet:
        case EHumidityType::Wettest:
            return EBiomeType::Savanna; // Wooded Badlands -> Savanna as it's dry but with some wood
        default: return EBiomeType::Desert;
    }
}

EBiomeType UTerrainGenerator::MapShatteredBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const {
    // Highly varied, often windswept versions of other biomes.
    // Simplified: just map to a more rugged/exposed version or similar to MiddleBiome.
    if (Temp <= ETemperatureType::Cold && Hum <= EHumidityType::Dry)
        return EBiomeType::Tundra; // Windswept Gravelly Hills -> Tundra

    // Fallback for simplicity
    return MapMiddleBiome(Temp, Hum, Weirdness_neg1_1);
}

// --- Data Accessors ---
float UTerrainGenerator::GetHeightData(int GlobalX, int GlobalY) const
{
	if (!bIsInitialized) return 0.0f;
	return HeightMap.GetData(GlobalX, GlobalY);
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

// --- Helper Functions ---

FBiomeSettings* UTerrainGenerator::GetBiomeSettings(EBiomeType BiomeType) const
{
	if (!BiomesTable) return nullptr;
	FName BiomeName = BiomeTypeToFName(BiomeType);
	return BiomesTable->FindRow<FBiomeSettings>(BiomeName, TEXT("Lookup Biome Settings"));
}

EBiomeType UTerrainGenerator::GetBiomeTypeFromMap(int GlobalX, int GlobalY) const
{
	if (!bIsInitialized) return EBiomeType::Grassland;
	return static_cast<EBiomeType>(FMath::RoundToInt(BiomeIDMap.GetData(GlobalX, GlobalY)));
}