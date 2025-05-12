#include "Objects/TerrainGenerator.h"
#include "FastNoiseWrapper.h"
#include "Actors/ChunkWorld.h"
#include "Structs/ChunkColumn.h"
#include "Structs/ChunkData.h"
#include "Engine/DataTable.h"
#include "Math/UnrealMathUtility.h"
#include "Objects/FoliageGenerator.h"
#include "Curves/CurveFloat.h" // Include for UCurveFloat

UTerrainGenerator::UTerrainGenerator()
{
	PrimaryComponentTick.bCanEverTick = false;
	FoliageGenerator = CreateDefaultSubobject<UFoliageGenerator>("FoliageGenerator");
}

void UTerrainGenerator::BeginPlay()
{
	Super::BeginPlay();

	ParentWorld = Cast<AChunkWorld>(GetOwner());
	if (!ParentWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("UTerrainGenerator needs to be owned by an AChunkWorld Actor!"));
		return;
	}

	if (!bNoiseInitialized)
	{
		UE_LOG(LogTemp, Log, TEXT("UTerrainGenerator: Initializing noise generators..."));
		InitializeNoise(); // Set up noise instances ONCE
		bNoiseInitialized = true; // Mark noise as ready
		UE_LOG(LogTemp, Log, TEXT("UTerrainGenerator: Noise generators initialized."));
	}

    // REMOVED: Call to GenerateWorldData() and related logic.
}


void UTerrainGenerator::InitializeNoise()
{
    // Ensure ParentWorld is valid before accessing Seed
    if (!ParentWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot initialize noise, ParentWorld is null."));
        return;
    }
	SetupNoise(ContinentalnessNoise, ContinentalnessNoiseSettings);
	SetupNoise(ErosionNoise, ErosionNoiseSettings);
	SetupNoise(WeirdnessNoise, WeirdnessNoiseSettings);
	// If PV uses Weirdness noise, no separate setup needed. If separate:
    // SetupNoise(PeaksValleysNoise, PeaksValleysNoiseSettings);
	SetupNoise(TemperatureNoise, TemperatureNoiseSettings);
	SetupNoise(HumidityNoise, HumidityNoiseSettings);
}

// SetupNoise remains the same as before
void UTerrainGenerator::SetupNoise(TObjectPtr<UFastNoiseWrapper>& Noise, const UNoiseOctaveSettingsAsset* Settings)
{
	if (!Settings) {
		UE_LOG(LogTemp, Warning, TEXT("Missing Noise Settings Asset for a noise type!"));
		return;
	}
	if (!Noise)
	{
		Noise = NewObject<UFastNoiseWrapper>(this);
	}
    // Need ParentWorld for Seed access
    if (!ParentWorld) {
        UE_LOG(LogTemp, Error, TEXT("Cannot setup noise '%s', ParentWorld is null."), *Settings->GetName());
        return;
    }
	Noise->SetupFastNoise(
		Settings->NoiseType,
		ParentWorld->Seed + Settings->SeedOffset, // All seeds are offset by the world seed
		Settings->Frequency,
		Settings->Interpolation,
		Settings->FractalType,
		Settings->Octaves,
		Settings->Lacunarity,
		Settings->Gain
	);
}

// REMOVED: GenerateWorldData() function and its callees (GenerateHeightMap, SimulateClimate, ClassifyBiomes)
//          The logic is moved into GenerateColumnData.


// Calculates ALL data for a single column at GlobalX, GlobalY
FChunkColumn UTerrainGenerator::GenerateColumnData(int GlobalX, int GlobalY)
{
	const int ChunkHeight = FChunkData::GetChunkHeight(this); // Get chunk height config
	FChunkColumn Column(ChunkHeight, GlobalX, GlobalY); // Create column structure

	if (!bNoiseInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateColumnData called before noise was initialized for (%d, %d)!"), GlobalX, GlobalY);
		// Optionally return a default column (e.g., all stone)
        for (int z = 0; z < ChunkHeight; ++z) Column.Blocks[z] = EBlock::Stone;
        Column.Height = FMath::Clamp(FMath::RoundToInt(TerrainBaseHeight), 0, ChunkHeight - 1);
		return Column;
	}

	// --- 1. Calculate Terrain Parameters (Cont, Ero, Weird, PV) ---
    float RawCont = ContinentalnessNoise ? ContinentalnessNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
	float RawErosion = ErosionNoise ? ErosionNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
	float RawWeirdness = WeirdnessNoise ? WeirdnessNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
    // If PV uses separate noise: float RawPV = PeaksValleysNoise->GetNoise2D(GlobalX, GlobalY);
    // Otherwise, derive from RawWeirdness as before:
    float PV_neg1_1 = 1.0f - FMath::Abs((3.0f * FMath::Abs(RawWeirdness)) - 2.0f);
	float PV01_forSpline = (PV_neg1_1 + 1.f) / 2.f;

    // Normalize raw noises to [0, 1] for spline input
	float Cont01 = (RawCont + 1.f) / 2.f;
	float Ero01 = (RawErosion + 1.f) / 2.f;
	float Weird01 = (RawWeirdness + 1.f) / 2.f;

    // Apply Splines
    float SplinedContinentalness, SplinedErosion, SplinedWeirdness, SplinedPeaksValleys;
    ApplySpline(ContinentalnessSpline, Cont01, SplinedContinentalness);
	ApplySpline(ErosionSpline, Ero01, SplinedErosion);
	ApplySpline(WeirdnessSpline, Weird01, SplinedWeirdness);
    ApplySpline(PeaksValleysSpline, PV01_forSpline, SplinedPeaksValleys); // Use PV derived from Weirdness

    FTerrainParameterData TerrainParams(SplinedContinentalness, SplinedErosion, SplinedWeirdness, SplinedPeaksValleys);
    Column.SetGenerationData(TerrainParams); // Store calculated parameters in the column

    // --- 2. Calculate Height ---
    // Remap splined [0,1] values back to [-1,1] style for weighted sum
    float ContRemapped = Remap01toNeg11(SplinedContinentalness);
    float PVRemapped = Remap01toNeg11(SplinedPeaksValleys);
    // Add Erosion if it affects height: float EroRemapped = Remap01toNeg11(SplinedErosion);

    float BaseNoise =
        ContRemapped * ContinentalnessWeight +
        PVRemapped   * PeaksValleysWeight;
        // + EroRemapped * ErosionWeight; // Add if erosion impacts base height noise

    // Flatten factor based on Erosion
    float FlattenFactor = FMath::Clamp(1.0f - SplinedErosion * ErosionWeight, 0.f, 1.f); // Original formula used SplinedErosion here directly

    float HeightNoise = BaseNoise * FlattenFactor;
    float AbsoluteHeight = TerrainBaseHeight + HeightNoise * TerrainAmplitude;

    int FinalBlockHeight = FMath::Clamp(FMath::RoundToInt(AbsoluteHeight), 0, ChunkHeight - 1);
    Column.Height = FinalBlockHeight; // Store calculated height in the column

    // --- 3. Simulate Climate (Temperature & Humidity) for this column ---
    // Temperature
    float Temp01 = 0.5f; // Default
    if (TemperatureNoise) {
        float RawTemp = TemperatureNoise->GetNoise2D(GlobalX, GlobalY);
	    Temp01 = (RawTemp + 1.f) / 2.f; // Normalize to [0, 1]
    }
	float HeightDifference = static_cast<float>(FinalBlockHeight) - TerrainBaseHeight;
	float AltitudeModifier = HeightDifference * AltitudeTemperatureFactor;
	float FinalTemp01 = FMath::Clamp(Temp01 - AltitudeModifier, 0.0f, 1.0f);
    Column.Temperature = FinalTemp01; // Store calculated temperature

    // Humidity
    float Humid01 = 0.5f; // Default
    if (HumidityNoise) {
        float RawHumidity = HumidityNoise->GetNoise2D(GlobalX, GlobalY);
	    Humid01 = (RawHumidity + 1.f) / 2.f; // Normalize to [0, 1]
        // Apply spline if you have one for Humidity
    }
	Column.Humidity = FMath::Clamp(Humid01, 0.0f, 1.0f); // Store calculated humidity

    // --- 4. Classify Biome ---
    // Remap calculated parameters to [-1, 1] for categorization
    float Cont_neg1_1 = Remap01toNeg11(TerrainParams.Continentalness);
    float Ero_neg1_1 = Remap01toNeg11(TerrainParams.Erosion);
    float Weird_neg1_1 = Remap01toNeg11(TerrainParams.Weirdness);
    float PV_neg1_1_biome = Remap01toNeg11(TerrainParams.PeaksValleys); // Use the final splined PV
    float Temp_neg1_1 = Remap01toNeg11(FinalTemp01);
    float Humid_neg1_1 = Remap01toNeg11(Column.Humidity); // Use final humidity

    // Categorize based on thresholds
    ETemperatureType CatTemp = CategorizeTemperature(Temp_neg1_1);
    EHumidityType CatHumid = CategorizeHumidity(Humid_neg1_1);
    EContinentalnessType CatCont = CategorizeContinentalness(Cont_neg1_1);
    EErosionType CatEro = CategorizeErosion(Ero_neg1_1);
    EPVType CatPV = CategorizePV(PV_neg1_1_biome);

    FCategorizedBiomeInputs BiomeInputs(CatTemp, CatHumid, CatCont, CatEro, CatPV, Weird_neg1_1); // Pass raw weirdness [-1,1] if needed by mappers

    EBiomeType FinalBiome = DetermineBiomeType(BiomeInputs);
    Column.SetBiomeType(FinalBiome); // Store calculated biome type

	return Column; // Return the fully calculated column data
}


// CalculateTerrainParameters is now just a helper returning the struct, not saving to map
FTerrainParameterData UTerrainGenerator::CalculateTerrainParameters(int GlobalX, int GlobalY) const
{
    // This function is now effectively integrated into GenerateColumnData
    // Kept here for potential reuse or clarity if GenerateColumnData gets too long
    // Ensure noise objects are checked for validity if called separately.

    float RawCont = ContinentalnessNoise ? ContinentalnessNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
	float RawErosion = ErosionNoise ? ErosionNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
	float RawWeirdness = WeirdnessNoise ? WeirdnessNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;

	float Cont01 = (RawCont + 1.f) / 2.f;
	float Ero01 = (RawErosion + 1.f) / 2.f;
	float Weird01 = (RawWeirdness + 1.f) / 2.f;

    float PV_neg1_1 = 1.0f - FMath::Abs((3.0f * FMath::Abs(RawWeirdness)) - 2.0f);
	float PV01_forSpline = (PV_neg1_1 + 1.f) / 2.f;

    float SplinedContinentalness, SplinedErosion, SplinedWeirdness, SplinedPeaksValleys;
    ApplySpline(ContinentalnessSpline, Cont01, SplinedContinentalness);
	ApplySpline(ErosionSpline, Ero01, SplinedErosion);
	ApplySpline(WeirdnessSpline, Weird01, SplinedWeirdness);
    ApplySpline(PeaksValleysSpline, PV01_forSpline, SplinedPeaksValleys);

    return FTerrainParameterData(SplinedContinentalness, SplinedErosion, SplinedWeirdness, SplinedPeaksValleys);
}

// ApplySpline remains the same, make const
void UTerrainGenerator::ApplySpline(const UCurveFloat* Spline, float InValue, float& OutValue) const
{
	if (Spline)
	{
		OutValue = Spline->GetFloatValue(InValue);
	}
	else
	{
		// If no spline, pass through the [0,1] value
		OutValue = InValue; // Changed from (InValue + 1.0f) / 2.0f; assuming InValue is already 0..1
	}
}


// --- PopulateColumnBlocks & DecorateChunkWithFoliage remain largely the same ---
// They operate on the FChunkColumn data passed to them.

void UTerrainGenerator::PopulateColumnBlocks(FChunkColumn& ColumnData)
{
	FBiomeSettings* BiomeSettings = GetBiomeSettings(ColumnData.GetBiomeType());
    const int ChunkHeight = FChunkData::GetChunkHeight(this); // Get max height

	if (!BiomeSettings)
	{
		// Handle default block assignment (e.g., stone up to height)
        for(int z = 0; z <= ColumnData.Height && z < ChunkHeight; ++z) {
            if (z >= 0) ColumnData.Blocks[z] = EBlock::Stone;
        }
		return;
	}

	int CurrentHeight = ColumnData.Height;
	for (const auto& Layer : BiomeSettings->Layers)
	{
		if (Layer.LayerThickness <= 0) continue;

        // Calculate the bottom Z index for this layer (inclusive)
		int LayerBottomHeight = CurrentHeight - (Layer.LayerThickness -1); // Subtract thickness-1 because CurrentHeight is inclusive top

		for (int z = CurrentHeight; z >= LayerBottomHeight; --z) // Iterate downwards
		{
            // Ensure Z is within valid array bounds [0, ChunkHeight-1]
			if (z >= 0 && z < ChunkHeight) {
                 ColumnData.Blocks[z] = Layer.BlockType;
            } else {
                 // Stop if we go below z=0
                 break;
            }
		}
		CurrentHeight = LayerBottomHeight - 1; // Next layer starts below the current one
		if (CurrentHeight < 0) break; // Stop if we've filled below z=0
	}

    // Fill remaining space below layers (down to z=0) with stone
    if (CurrentHeight >= 0) {
        for (int z = CurrentHeight; z >= 0; --z) {
             if (z < ChunkHeight) { // Extra safety check
                 ColumnData.Blocks[z] = EBlock::Stone;
             }
        }
    }

    // Place water if below threshold
	if (ColumnData.Height < WaterThreshold) {
		for (int z = ColumnData.Height + 1; z <= WaterThreshold; ++z) {
			if (z >= 0 && z < ChunkHeight) {
                 ColumnData.Blocks[z] = EBlock::Water;
             }
		}
	}
}


void UTerrainGenerator::DecorateChunkWithFoliage(TArray<FChunkColumn>& InOutChunkColumns,
	const FIntVector2& ChunkGridPosition, const FRandomStream& WorldFoliageStreamBase) const
{
	if (!bNoiseInitialized || InOutChunkColumns.IsEmpty() || !FoliageGenerator) return; // Check noise init instead of general init

    // Rest of the function remains the same as it operates on the provided columns
    int ChunkSize = FChunkData::GetChunkSize(this);
	const int ChunkHeight = FChunkData::GetChunkHeight(this);

    for (int Y_Local = 0; Y_Local < ChunkSize; ++Y_Local)
    {
        for (int X_Local = 0; X_Local < ChunkSize; ++X_Local)
        {
            int ColumnIndex = FChunkData::GetColumnIndex(this, X_Local, Y_Local); // Pass ChunkSize
            if (!InOutChunkColumns.IsValidIndex(ColumnIndex)) continue; // Bounds check

            FChunkColumn& CurrentColumn = InOutChunkColumns[ColumnIndex];
            EBiomeType BiomeType = CurrentColumn.GetBiomeType();
            FBiomeSettings* BiomeInfo = GetBiomeSettings(BiomeType);

            if (!BiomeInfo || BiomeInfo->FoliageRules.IsEmpty()) continue; // Check if biome has foliage

            FRandomStream ColumnFoliageDecisionStream;
            int32 GlobalX = ChunkGridPosition.X * ChunkSize + X_Local;
            int32 GlobalY = ChunkGridPosition.Y * ChunkSize + Y_Local;

            // Consistent seed generation per column
            ColumnFoliageDecisionStream.Initialize(
                WorldFoliageStreamBase.GetCurrentSeed() ^ GlobalX ^ (GlobalY << 16) ^ (GlobalY >> 16)
            );

            FoliageGenerator->AttemptPlaceFoliageAt(
                InOutChunkColumns,
                X_Local, Y_Local,
                BiomeInfo,
                ColumnFoliageDecisionStream,
                ChunkSize,
                ChunkHeight // Pass ChunkHeight
            );
        }
    }
}


// DetermineBiomeType and its helpers (Categorize*, Map*Biome) remain the same logic
// but should be marked const as they don't modify state.
EBiomeType UTerrainGenerator::DetermineBiomeType(const FCategorizedBiomeInputs& Params) const
{
    // ... (Keep existing logic) ...
     // Stage 1: Non-Inland Biomes (Oceans, Mushroom Fields)
    if (Params.Continentalness == EContinentalnessType::MushroomFields) return EBiomeType::Woodland; // Or a specific Mushroom biome if added

    if (Params.Continentalness == EContinentalnessType::DeepOcean ||
        Params.Continentalness == EContinentalnessType::Ocean) {
        switch (Params.Temperature) {
            case ETemperatureType::Coldest: return EBiomeType::Ice;      // Frozen Ocean
            case ETemperatureType::Cold:    return EBiomeType::Tundra;     // Cold Ocean
            case ETemperatureType::Temperate: // Lukewarm Ocean
            case ETemperatureType::Warm:      // Warm Ocean
            case ETemperatureType::Hot:       // Warm Ocean too?
                return EBiomeType::Grassland; // Representing Generic Ocean floor
            default: return EBiomeType::Grassland;
        }
    }

     // Stage 2: Inland Biomes
    // Valleys (Rivers)
    if (Params.PV == EPVType::Valleys) {
        if (Params.Temperature == ETemperatureType::Coldest) return EBiomeType::Ice; // Frozen River
        // Specific Swamp/Mangrove check (Example: High Humidity, Low Erosion)
        if (Params.Humidity >= EHumidityType::Wet && Params.Erosion <= EErosionType::Level1 && (Params.Temperature == ETemperatureType::Warm || Params.Temperature == ETemperatureType::Hot)) {
             return EBiomeType::TropicalRainforest; // Using this as a proxy for Swamp/Mangrove
        }
         // Dripstone caves? (Maybe tied to high erosion in valleys?) - Needs more specific rules
        return EBiomeType::Grassland; // Default River bank
    }

     // Peaks
    if (Params.PV == EPVType::Peaks || Params.PV == EPVType::High) {
         if (Params.Temperature <= ETemperatureType::Temperate) return EBiomeType::Ice; // Frozen/Jagged/Stony Peaks
         if (Params.Temperature == ETemperatureType::Warm) return EBiomeType::Tundra; // Stony Peaks / Grove (Use Tundra for now)
         if (Params.Temperature == ETemperatureType::Hot) {
             // Badlands Check (Low Erosion)
             if (Params.Erosion <= EErosionType::Level1) return EBiomeType::Desert; // Eroded Badlands / Wooded Badlands Peak
             else return EBiomeType::Savanna; // Stony Peaks in hot climate but not badlands erosion
         }
    }

     // Coast / Beach
    if (Params.Continentalness == EContinentalnessType::Coast && Params.PV == EPVType::Low) {
         return MapBeachBiome(Params.Temperature);
    }

     // Default to Middle Biome mapping (Plains, Forests, Deserts, Savannas, Taigas etc.)
    return MapMiddleBiome(Params.Temperature, Params.Humidity, Params.WeirdnessValue);
}
ETemperatureType UTerrainGenerator::CategorizeTemperature(float TempValue_neg1_1) const { /* ... Keep Logic ... */
    if (TempValue_neg1_1 < TemperatureThresholds.ColdestValue) return ETemperatureType::Coldest;
	if (TempValue_neg1_1 < TemperatureThresholds.ColderValue) return ETemperatureType::Cold;
	if (TempValue_neg1_1 < TemperatureThresholds.TemperateValue) return ETemperatureType::Temperate;
	if (TempValue_neg1_1 < TemperatureThresholds.WarmValue) return ETemperatureType::Warm;
	return ETemperatureType::Hot;
 }
EHumidityType UTerrainGenerator::CategorizeHumidity(float HumidityValue_neg1_1) const { /* ... Keep Logic ... */
    if (HumidityValue_neg1_1 < HumidityThresholds.DryestValue) return EHumidityType::Dryest;
	if (HumidityValue_neg1_1 < HumidityThresholds.DryValue) return EHumidityType::Dry;
	if (HumidityValue_neg1_1 < HumidityThresholds.MediumValue) return EHumidityType::Medium;
	if (HumidityValue_neg1_1 < HumidityThresholds.WetValue) return EHumidityType::Wet;
	return EHumidityType::Wettest;
}
EContinentalnessType UTerrainGenerator::CategorizeContinentalness(float ContinentalnessValue_neg1_1) const { /* ... Keep Logic ... */
    if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.MushroomFieldsValue) return EContinentalnessType::MushroomFields;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.DeepOceanValue) return EContinentalnessType::DeepOcean;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.OceanValue) return EContinentalnessType::Ocean;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.CoastValue) return EContinentalnessType::Coast;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.NearInlandValue) return EContinentalnessType::NearInland;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.MidInlandValue) return EContinentalnessType::MidInland;
	return EContinentalnessType::FarInland;
}
EErosionType UTerrainGenerator::CategorizeErosion(float ErosionValue_neg1_1) const { /* ... Keep Logic ... */
    if (ErosionValue_neg1_1 < ErosionThresholds.E0Value) return EErosionType::Level0;
	if (ErosionValue_neg1_1 < ErosionThresholds.E1Value) return EErosionType::Level1;
	if (ErosionValue_neg1_1 < ErosionThresholds.E2Value) return EErosionType::Level2;
	if (ErosionValue_neg1_1 < ErosionThresholds.E3Value) return EErosionType::Level3;
	if (ErosionValue_neg1_1 < ErosionThresholds.E4Value) return EErosionType::Level4;
	if (ErosionValue_neg1_1 < ErosionThresholds.E5Value) return EErosionType::Level5;
	return EErosionType::Level6;
}
EPVType UTerrainGenerator::CategorizePV(float PVValue_neg1_1) const { /* ... Keep Logic ... */
    if (PVValue_neg1_1 < PeaksValleysThresholds.ValleysValue) return EPVType::Valleys;
	if (PVValue_neg1_1 < PeaksValleysThresholds.LowValue) return EPVType::Low;
	if (PVValue_neg1_1 < PeaksValleysThresholds.MidValue) return EPVType::Mid;
	if (PVValue_neg1_1 < PeaksValleysThresholds.HighValue) return EPVType::High;
	return EPVType::Peaks;
}
EBiomeType UTerrainGenerator::MapMiddleBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const { /* ... Keep Logic ... */
    switch (Temp)
    {
    case ETemperatureType::Coldest: // T=0 -> Snowy Biomes
        // Weirdness check for Ice Spikes vs Snowy Plains/Taiga
        if (Weirdness_neg1_1 > 0.1f) return EBiomeType::Ice; // Ice Spikes (High positive Weirdness)
        switch (Hum) {
            case EHumidityType::Dryest:
            case EHumidityType::Dry:
            case EHumidityType::Medium: return EBiomeType::Tundra;    // Snowy Plains
            case EHumidityType::Wet:
            case EHumidityType::Wettest: return EBiomeType::BorealForest; // Snowy Taiga
            default: return EBiomeType::Tundra;
        }
    case ETemperatureType::Cold: // T=1 -> Cool Biomes
        switch (Hum) {
            case EHumidityType::Dryest:
            case EHumidityType::Dry:    return EBiomeType::Grassland;       // Plains
            case EHumidityType::Medium: return EBiomeType::Woodland;        // Forest
            case EHumidityType::Wet:    return EBiomeType::BorealForest;    // Taiga
            case EHumidityType::Wettest:return EBiomeType::BorealForest;    // Old Growth Taiga -> BorealForest for now
            default: return EBiomeType::Grassland;
        }
    case ETemperatureType::Temperate: // T=2 -> Temperate Biomes
        switch (Hum) {
            case EHumidityType::Dryest: // Flower Forest? (High positive Weirdness) vs Plains
                 return Weirdness_neg1_1 > 0.1f ? EBiomeType::SeasonalForest : EBiomeType::Grassland;
            case EHumidityType::Dry:    return EBiomeType::Grassland;       // Plains
            case EHumidityType::Medium: // Forest? (Normal Weirdness) vs Plains (High positive Weirdness)
                 return Weirdness_neg1_1 > 0.1f ? EBiomeType::Grassland : EBiomeType::Woodland;
            case EHumidityType::Wet:    return EBiomeType::SeasonalForest;  // Birch Forest
            case EHumidityType::Wettest:return EBiomeType::TemperateRainforest; // Dark Forest
            default: return EBiomeType::Woodland;
        }
    case ETemperatureType::Warm: // T=3 -> Dry/Warm Biomes
        switch (Hum) {
             // Savanna variants based on weirdness?
            case EHumidityType::Dryest:
            case EHumidityType::Dry:
            case EHumidityType::Medium: return EBiomeType::Savanna;         // Savanna / Savanna Plateau
            // Jungle variants based on weirdness?
            case EHumidityType::Wet:
            case EHumidityType::Wettest: return EBiomeType::TropicalRainforest; // Jungle / Sparse Jungle / Bamboo Jungle
            default: return EBiomeType::Savanna;
        }
    case ETemperatureType::Hot: // T=4 -> Hot/Arid Biomes
        // Desert vs Badlands (based on Erosion, handled elsewhere or refine here)
        return EBiomeType::Desert; // Default to Desert
    default:
        return EBiomeType::Grassland; // Fallback
    }
}
EBiomeType UTerrainGenerator::MapPlateauBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const { /* ... Keep Logic ... */
    // Example: Savanna Plateau vs standard Savanna
    if (Temp == ETemperatureType::Warm && Hum <= EHumidityType::Medium) return EBiomeType::Savanna; // Savanna Plateau -> Savanna

    // Example: Wooded Badlands Plateau / Badlands
     if (Temp == ETemperatureType::Hot) return EBiomeType::Desert; // Badlands Plateau -> Desert

    // Fallback to middle biome logic for other plateau types for simplicity
    return MapMiddleBiome(Temp, Hum, Weirdness_neg1_1);
}
EBiomeType UTerrainGenerator::MapBeachBiome(ETemperatureType Temp) const { /* ... Keep Logic ... */
    switch (Temp) {
        case ETemperatureType::Coldest: return EBiomeType::Tundra; // Snowy Beach
        case ETemperatureType::Cold:
        case ETemperatureType::Temperate:
        case ETemperatureType::Warm: return EBiomeType::Grassland; // Beach (use Grassland blocks for now)
        case ETemperatureType::Hot: return EBiomeType::Desert;    // Desert Beach (use Desert blocks for now)
        default: return EBiomeType::Grassland;
    }
}
EBiomeType UTerrainGenerator::MapShatteredBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const { /* ... Keep Logic ... */
     // Example: Shattered Savanna (Warm, Dry/Medium Humidity)
    if (Temp == ETemperatureType::Warm && Hum <= EHumidityType::Medium) return EBiomeType::Savanna;
    // Example: Gravelly Mountains (Cold, Dry)
    if (Temp <= ETemperatureType::Cold && Hum <= EHumidityType::Dry) return EBiomeType::Tundra; // Use Tundra as proxy

    // Fallback for other shattered variants
    return MapMiddleBiome(Temp, Hum, Weirdness_neg1_1);
}


// GetBiomeSettings remains the same, make const
FBiomeSettings* UTerrainGenerator::GetBiomeSettings(EBiomeType BiomeType) const
{
	if (!BiomesTable) return nullptr;
	FName BiomeName = BiomeTypeToFName(BiomeType); // Assuming BiomeTypeToFName helper exists
    if (BiomeName == NAME_None) return nullptr; // Handle invalid enum to name conversion
	return BiomesTable->FindRow<FBiomeSettings>(BiomeName, TEXT("Lookup Biome Settings"));
}