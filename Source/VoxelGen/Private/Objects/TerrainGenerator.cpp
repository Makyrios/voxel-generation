#include "Objects/TerrainGenerator.h"
#include "FastNoiseWrapper.h"
#include "Actors/ChunkWorld.h"
#include "Structs/ChunkColumn.h"
#include "Structs/ChunkData.h"
#include "Engine/DataTable.h"
#include "Math/UnrealMathUtility.h"
#include "Objects/FoliageGenerator.h"
#include "Curves/CurveFloat.h" // Include for UCurveFloat
#include "Structs/NoiseGenerationPreset.h"

UTerrainGenerator::UTerrainGenerator()
{
	PrimaryComponentTick.bCanEverTick = false;
	FoliageGenerator = CreateDefaultSubobject<UFoliageGenerator>("FoliageGenerator");
}

void UTerrainGenerator::UpdateSeed(int32 NewSeed) const
{
	ContinentalnessNoise->SetSeed(NewSeed);
	ErosionNoise->SetSeed(NewSeed);
	WeirdnessNoise->SetSeed(NewSeed);
	TemperatureNoise->SetSeed(NewSeed);
	HumidityNoise->SetSeed(NewSeed);
}

void UTerrainGenerator::SetNoisePreset(UNoiseGenerationPreset* NewPreset)
{
    if (NewPreset && NewPreset != NoiseGenerationPreset)
    {
        // Clean up existing noise objects
        ContinentalnessNoise = nullptr;
        ErosionNoise = nullptr;
        WeirdnessNoise = nullptr;
        TemperatureNoise = nullptr;
        HumidityNoise = nullptr;

        // Set the new preset and reinitialize noise
        NoiseGenerationPreset = NewPreset;
        InitializeNoise();
    }
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
		InitializeNoise();
		bNoiseInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("UTerrainGenerator: Noise generators initialized."));
	}
}


void UTerrainGenerator::InitializeNoise()
{
    if (!ParentWorld)
    {
        return;
    }
	SetupNoise(ContinentalnessNoise, NoiseGenerationPreset->Continentalness);
	SetupNoise(ErosionNoise, NoiseGenerationPreset->Erosion);
	SetupNoise(WeirdnessNoise, NoiseGenerationPreset->Weirdness);
	SetupNoise(TemperatureNoise, NoiseGenerationPreset->Temperature);
	SetupNoise(HumidityNoise, NoiseGenerationPreset->Humidity);
}

// SetupNoise remains the same as before
void UTerrainGenerator::SetupNoise(TObjectPtr<UFastNoiseWrapper>& Noise, const UNoiseOctaveSettingsAsset* Settings)
{
	if (!Settings || !ParentWorld) return;
	if (!Noise)
	{
		Noise = NewObject<UFastNoiseWrapper>(this);
	}
    
	Noise->SetupFastNoise(
		Settings->NoiseType,
		ParentWorld->Seed + Settings->SeedOffset,
		Settings->Frequency,
		Settings->Interpolation,
		Settings->FractalType,
		Settings->Octaves,
		Settings->Lacunarity,
		Settings->Gain
	);
}

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
	
    float RawCont = ContinentalnessNoise ? ContinentalnessNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
	float RawErosion = ErosionNoise ? ErosionNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
	float RawWeirdness = WeirdnessNoise ? WeirdnessNoise->GetNoise2D(GlobalX, GlobalY) : 0.f;
    float PV_neg1_1 = 1.0f - FMath::Abs((3.0f * FMath::Abs(RawWeirdness)) - 2.0f);
	float PV01_forSpline = (PV_neg1_1 + 1.f) / 2.f;

    // Normalize raw noises to [0, 1] for spline input
	float Cont01 = (RawCont + 1.f) / 2.f;
	float Ero01 = (RawErosion + 1.f) / 2.f;
	float Weird01 = (RawWeirdness + 1.f) / 2.f;

    float SplinedContinentalness, SplinedErosion, SplinedWeirdness, SplinedPeaksValleys;
    ApplySpline(ContinentalnessSpline, Cont01, SplinedContinentalness);
	ApplySpline(ErosionSpline, Ero01, SplinedErosion);
	ApplySpline(WeirdnessSpline, Weird01, SplinedWeirdness);
    ApplySpline(PeaksValleysSpline, PV01_forSpline, SplinedPeaksValleys);

    FTerrainParameterData TerrainParams(SplinedContinentalness, SplinedErosion, SplinedWeirdness, SplinedPeaksValleys);
    Column.SetGenerationData(TerrainParams); // Store calculated parameters in the column

    // Remap splined [0,1] values back to [-1,1] style for weighted sum
    float ContRemapped = Remap01toNeg11(SplinedContinentalness);
    float PVRemapped = Remap01toNeg11(SplinedPeaksValleys);

    float BaseNoise =
        ContRemapped * ContinentalnessWeight +
        PVRemapped   * PeaksValleysWeight;

    // Flatten factor based on Erosion
    float FlattenFactor = FMath::Clamp(1.0f - SplinedErosion * ErosionWeight, 0.f, 1.f);

    float HeightNoise = BaseNoise * FlattenFactor;
    float AbsoluteHeight = TerrainBaseHeight + HeightNoise * TerrainAmplitude;

    int FinalBlockHeight = FMath::Clamp(FMath::RoundToInt(AbsoluteHeight), 0, ChunkHeight - 1);
    Column.Height = FinalBlockHeight;


    float Temp01 = 0.5f;
    if (TemperatureNoise) {
        float RawTemp = TemperatureNoise->GetNoise2D(GlobalX, GlobalY);
	    Temp01 = (RawTemp + 1.f) / 2.f;
    }
	float HeightDifference = static_cast<float>(FinalBlockHeight) - TerrainBaseHeight;
	float AltitudeModifier = HeightDifference * AltitudeTemperatureFactor;
	float FinalTemp01 = FMath::Clamp(Temp01 - AltitudeModifier, 0.0f, 1.0f);
    Column.Temperature = FinalTemp01;


    float Humid01 = 0.5f;
    if (HumidityNoise) {
        float RawHumidity = HumidityNoise->GetNoise2D(GlobalX, GlobalY);
	    Humid01 = (RawHumidity + 1.f) / 2.f;
    }
	Column.Humidity = FMath::Clamp(Humid01, 0.0f, 1.0f);
	
    // Remap calculated parameters to [-1, 1] for categorization
    float Cont_neg1_1 = Remap01toNeg11(TerrainParams.Continentalness);
    float Ero_neg1_1 = Remap01toNeg11(TerrainParams.Erosion);
    float Weird_neg1_1 = Remap01toNeg11(TerrainParams.Weirdness);
    float PV_neg1_1_biome = Remap01toNeg11(TerrainParams.PeaksValleys);
    float Temp_neg1_1 = Remap01toNeg11(FinalTemp01);
    float Humid_neg1_1 = Remap01toNeg11(Column.Humidity);

    // Categorize based on thresholds
    ETemperatureType CatTemp = CategorizeTemperature(Temp_neg1_1);
    EHumidityType CatHumid = CategorizeHumidity(Humid_neg1_1);
    EContinentalnessType CatCont = CategorizeContinentalness(Cont_neg1_1);
    EErosionType CatEro = CategorizeErosion(Ero_neg1_1);
    EPVType CatPV = CategorizePV(PV_neg1_1_biome);

    FCategorizedBiomeInputs BiomeInputs(CatTemp, CatHumid, CatCont, CatEro, CatPV, Weird_neg1_1);

    EBiomeType FinalBiome = DetermineBiomeType(BiomeInputs);
    Column.SetBiomeType(FinalBiome);

	return Column;
}


FTerrainParameterData UTerrainGenerator::CalculateTerrainParameters(int GlobalX, int GlobalY) const
{
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

void UTerrainGenerator::ApplySpline(const UCurveFloat* Spline, float InValue, float& OutValue) const
{
	if (Spline)
	{
		OutValue = Spline->GetFloatValue(InValue);
	}
	else
	{
		OutValue = InValue;
	}
}

void UTerrainGenerator::PopulateColumnBlocks(FChunkColumn& ColumnData) const
{
	FBiomeSettings* BiomeSettings = GetBiomeSettings(ColumnData.GetBiomeType());
    const int ChunkHeight = FChunkData::GetChunkHeight(this);

	if (!BiomeSettings)
	{
        for(int z = 0; z <= ColumnData.Height && z < ChunkHeight; ++z)
        {
            if (z >= 0) ColumnData.Blocks[z] = EBlock::Stone;
        }
		return;
	}

	int CurrentHeight = ColumnData.Height;
	for (const auto& Layer : BiomeSettings->Layers)
	{
		if (Layer.LayerThickness <= 0) continue;

        // Calculate the bottom Z index for this layer (inclusive)
		int LayerBottomHeight = CurrentHeight - (Layer.LayerThickness -1);

		for (int z = CurrentHeight; z >= LayerBottomHeight; --z)
		{
			if (z >= 0 && z < ChunkHeight)
			{
				ColumnData.Blocks[z] = Layer.BlockType;
            }
		    else
            {
                break;
            }
		}
		CurrentHeight = LayerBottomHeight - 1;
		if (CurrentHeight < 0) break;
	}

    if (CurrentHeight >= 0) {
        for (int z = CurrentHeight; z >= 0; --z)
        {
            if (z < ChunkHeight)
            {
                ColumnData.Blocks[z] = EBlock::Stone;
            }
        }
    }

	if (ColumnData.Height < WaterThreshold) {
		for (int z = ColumnData.Height + 1; z <= WaterThreshold; ++z)
		{
			if (z >= 0 && z < ChunkHeight)
			{
                 ColumnData.Blocks[z] = EBlock::Water;
			}
		}
	}
}


void UTerrainGenerator::DecorateChunkWithFoliage(TArray<FChunkColumn>& InOutChunkColumns,
	const FIntVector2& ChunkGridPosition, const FRandomStream& WorldFoliageStreamBase) const
{
	if (!bNoiseInitialized || InOutChunkColumns.IsEmpty() || !FoliageGenerator) return;

    int ChunkSize = FChunkData::GetChunkSize(this);
	const int ChunkHeight = FChunkData::GetChunkHeight(this);

    for (int Y_Local = 0; Y_Local < ChunkSize; ++Y_Local)
    {
        for (int X_Local = 0; X_Local < ChunkSize; ++X_Local)
        {
            int ColumnIndex = FChunkData::GetColumnIndex(this, X_Local, Y_Local);
            if (!InOutChunkColumns.IsValidIndex(ColumnIndex)) continue;

            FChunkColumn& CurrentColumn = InOutChunkColumns[ColumnIndex];
            EBiomeType BiomeType = CurrentColumn.GetBiomeType();
            FBiomeSettings* BiomeInfo = GetBiomeSettings(BiomeType);

            if (!BiomeInfo || BiomeInfo->FoliageRules.IsEmpty()) continue;

            FRandomStream ColumnFoliageDecisionStream;
            int32 GlobalX = ChunkGridPosition.X * ChunkSize + X_Local;
            int32 GlobalY = ChunkGridPosition.Y * ChunkSize + Y_Local;

            ColumnFoliageDecisionStream.Initialize(
                WorldFoliageStreamBase.GetCurrentSeed() ^ GlobalX ^ (GlobalY << 16) ^ (GlobalY >> 16)
            );

            FoliageGenerator->AttemptPlaceFoliageAt(
                InOutChunkColumns,
                X_Local, Y_Local,
                BiomeInfo,
                ColumnFoliageDecisionStream,
                ChunkSize,
                ChunkHeight
            );
        }
    }
}

EBiomeType UTerrainGenerator::DetermineBiomeType(const FCategorizedBiomeInputs& Params) const
{
     // Non-Inland Biomes (Oceans, Mushroom Fields)
    if (Params.Continentalness == EContinentalnessType::MushroomFields) return EBiomeType::Woodland;

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

    // Inland Biomes
    // Valleys (Rivers)
    if (Params.PV == EPVType::Valleys) {
        if (Params.Temperature == ETemperatureType::Coldest) return EBiomeType::Ice; // Frozen River
        return EBiomeType::Grassland;
    }

     // Default to Middle Biome mapping (Plains, Forests, Deserts, Savannas, Taigas etc.)
    return MapMiddleBiome(Params.Temperature, Params.Humidity, Params.WeirdnessValue);
}
ETemperatureType UTerrainGenerator::CategorizeTemperature(float TempValue_neg1_1) const {
    if (TempValue_neg1_1 < TemperatureThresholds.ColdestValue) return ETemperatureType::Coldest;
	if (TempValue_neg1_1 < TemperatureThresholds.ColderValue) return ETemperatureType::Cold;
	if (TempValue_neg1_1 < TemperatureThresholds.TemperateValue) return ETemperatureType::Temperate;
	if (TempValue_neg1_1 < TemperatureThresholds.WarmValue) return ETemperatureType::Warm;
	return ETemperatureType::Hot;
 }
EHumidityType UTerrainGenerator::CategorizeHumidity(float HumidityValue_neg1_1) const {
    if (HumidityValue_neg1_1 < HumidityThresholds.DryestValue) return EHumidityType::Dryest;
	if (HumidityValue_neg1_1 < HumidityThresholds.DryValue) return EHumidityType::Dry;
	if (HumidityValue_neg1_1 < HumidityThresholds.MediumValue) return EHumidityType::Medium;
	if (HumidityValue_neg1_1 < HumidityThresholds.WetValue) return EHumidityType::Wet;
	return EHumidityType::Wettest;
}
EContinentalnessType UTerrainGenerator::CategorizeContinentalness(float ContinentalnessValue_neg1_1) const {
    if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.MushroomFieldsValue) return EContinentalnessType::MushroomFields;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.DeepOceanValue) return EContinentalnessType::DeepOcean;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.OceanValue) return EContinentalnessType::Ocean;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.CoastValue) return EContinentalnessType::Coast;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.NearInlandValue) return EContinentalnessType::NearInland;
	if (ContinentalnessValue_neg1_1 < ContinentalnessThresholds.MidInlandValue) return EContinentalnessType::MidInland;
	return EContinentalnessType::FarInland;
}
EErosionType UTerrainGenerator::CategorizeErosion(float ErosionValue_neg1_1) const {
    if (ErosionValue_neg1_1 < ErosionThresholds.E0Value) return EErosionType::Level0;
	if (ErosionValue_neg1_1 < ErosionThresholds.E1Value) return EErosionType::Level1;
	if (ErosionValue_neg1_1 < ErosionThresholds.E2Value) return EErosionType::Level2;
	if (ErosionValue_neg1_1 < ErosionThresholds.E3Value) return EErosionType::Level3;
	if (ErosionValue_neg1_1 < ErosionThresholds.E4Value) return EErosionType::Level4;
	if (ErosionValue_neg1_1 < ErosionThresholds.E5Value) return EErosionType::Level5;
	return EErosionType::Level6;
}
EPVType UTerrainGenerator::CategorizePV(float PVValue_neg1_1) const {
    if (PVValue_neg1_1 < PeaksValleysThresholds.ValleysValue) return EPVType::Valleys;
	if (PVValue_neg1_1 < PeaksValleysThresholds.LowValue) return EPVType::Low;
	if (PVValue_neg1_1 < PeaksValleysThresholds.MidValue) return EPVType::Mid;
	if (PVValue_neg1_1 < PeaksValleysThresholds.HighValue) return EPVType::High;
	return EPVType::Peaks;
}
EBiomeType UTerrainGenerator::MapMiddleBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const {
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
            case EHumidityType::Wet:
            case EHumidityType::Wettest:return EBiomeType::BorealForest;    // orealForest for now
            default: return EBiomeType::Grassland;
        }
    case ETemperatureType::Temperate: // T=2 -> Temperate Biomes
        switch (Hum) {
            case EHumidityType::Dryest:
                 return Weirdness_neg1_1 > 0.1f ? EBiomeType::SeasonalForest : EBiomeType::Grassland;
            case EHumidityType::Dry:    return EBiomeType::Grassland;
            case EHumidityType::Medium:
                 return Weirdness_neg1_1 > 0.1f ? EBiomeType::Grassland : EBiomeType::Woodland;
            case EHumidityType::Wet:    return EBiomeType::SeasonalForest;
            case EHumidityType::Wettest:return EBiomeType::TemperateRainforest;
            default: return EBiomeType::Woodland;
        }
    case ETemperatureType::Warm:
        switch (Hum) {
            case EHumidityType::Dryest:
            case EHumidityType::Dry:
            case EHumidityType::Medium: return EBiomeType::Savanna;
            case EHumidityType::Wet:
            case EHumidityType::Wettest: return EBiomeType::TropicalRainforest;
            default: return EBiomeType::Savanna;
        }
    case ETemperatureType::Hot:
        return EBiomeType::Desert;
    default:
        return EBiomeType::Grassland;
    }
}
EBiomeType UTerrainGenerator::MapPlateauBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const {
    if (Temp == ETemperatureType::Warm && Hum <= EHumidityType::Medium) return EBiomeType::Savanna;
    if (Temp == ETemperatureType::Hot) return EBiomeType::Desert;

    return MapMiddleBiome(Temp, Hum, Weirdness_neg1_1);
}

EBiomeType UTerrainGenerator::MapShatteredBiome(ETemperatureType Temp, EHumidityType Hum, float Weirdness_neg1_1) const {
    if (Temp == ETemperatureType::Warm && Hum <= EHumidityType::Medium) return EBiomeType::Savanna;
    if (Temp <= ETemperatureType::Cold && Hum <= EHumidityType::Dry) return EBiomeType::Tundra;

    return MapMiddleBiome(Temp, Hum, Weirdness_neg1_1);
}

FBiomeSettings* UTerrainGenerator::GetBiomeSettings(EBiomeType BiomeType) const
{
	if (!BiomesTable) return nullptr;
	FName BiomeName = BiomeTypeToFName(BiomeType);
    if (BiomeName == NAME_None) return nullptr;
	return BiomesTable->FindRow<FBiomeSettings>(BiomeName, TEXT("Lookup Biome Settings"));
}