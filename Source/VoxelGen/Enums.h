#pragma once

enum class EDirection
{
	Forward,
	Right,
	Backward,
	Left,
	Up,
	Down
};

UENUM()
enum class EBlock
{
	Air UMETA(DisplayName = "Air"),
	Grass UMETA(DisplayName = "Grass"),
	Dirt UMETA(DisplayName = "Dirt"),
	Stone UMETA(DisplayName = "Stone"),
	Snow UMETA(DisplayName = "Snow"),
	Sand UMETA(DisplayName = "Sand"),
	Water UMETA(DisplayName = "Water"),
	OakLeaves UMETA(DisplayName = "Oak Leaves"),
	OakWood UMETA(DisplayName = "Oak Wood"),
	Redstone UMETA(DisplayName = "Redstone"),
	Redsand UMETA(DisplayName = "Red Sand")
};

UENUM()
enum class EBlockMaterialType
{
	Opaque UMETA(DisplayName = "Opaque"),
	Transparent UMETA(DisplayName = "Transparent"),
	Masked UMETA(DisplayName = "Masked")
};

UENUM(BlueprintType)
enum class ETemperatureType : uint8
{
	Coldest = 0		UMETA(DisplayName = "Coldest"),
	Cold = 1		UMETA(DisplayName = "Cold"),
	Temperate = 2	UMETA(DisplayName = "Temperate"),
	Warm = 3		UMETA(DisplayName = "Warm"),
	Hot = 4			UMETA(DisplayName = "Hot"),
};

UENUM(BlueprintType)
enum class EHumidityType : uint8
{
	Dryest = 0	UMETA(DisplayName = "Dryest"),
	Dry = 1		UMETA(DisplayName = "Dry"),
	Medium = 2	UMETA(DisplayName = "Medium"),
	Wet = 3		UMETA(DisplayName = "Wet"),
	Wettest = 4	UMETA(DisplayName = "Wettest"),
};

UENUM(BlueprintType)
enum class EContinentalnessType : uint8
{
	MushroomFields = 0 UMETA(DisplayName = "C: Mushroom Fields"),
	DeepOcean      = 1 UMETA(DisplayName = "C: Deep Ocean"),
	Ocean          = 2 UMETA(DisplayName = "C: Ocean"),
	Coast          = 3 UMETA(DisplayName = "C: Coast"),
	NearInland     = 4 UMETA(DisplayName = "C: Near Inland"),
	MidInland      = 5 UMETA(DisplayName = "C: Mid Inland"),
	FarInland      = 6 UMETA(DisplayName = "C: Far Inland")
};

UENUM(BlueprintType)
enum class EErosionType : uint8
{
	Level0 = 0 UMETA(DisplayName = "E0: (-1.0 to -0.78)"),
	Level1 = 1 UMETA(DisplayName = "E1: (-0.78 to -0.375)"),
	Level2 = 2 UMETA(DisplayName = "E2: (-0.375 to -0.2225)"),
	Level3 = 3 UMETA(DisplayName = "E3: (-0.2225 to 0.05)"),
	Level4 = 4 UMETA(DisplayName = "E4: (0.05 to 0.45)"),
	Level5 = 5 UMETA(DisplayName = "E5: (0.45 to 0.55)"),
	Level6 = 6 UMETA(DisplayName = "E6: (0.55 to 1.0)")
};

UENUM(BlueprintType)
enum class EPVType : uint8 // Peaks and Valleys
{
	Valleys = 0 UMETA(DisplayName = "PV: Valleys (-1.0 to -0.85)"),
	Low     = 1 UMETA(DisplayName = "PV: Low (-0.85 to -0.6)"),
	Mid     = 2 UMETA(DisplayName = "PV: Mid (-0.6 to 0.2)"),
	High    = 3 UMETA(DisplayName = "PV: High (0.2 to 0.7)"),  
	Peaks   = 4 UMETA(DisplayName = "PV: Peaks (0.7 to 1.0)")  
};

UENUM()
enum class EBiomeType : uint8
{
	Desert				UMETA(DisplayName = "Desert"),
	Savanna				UMETA(DisplayName = "Savanna"),
	TropicalRainforest	UMETA(DisplayName = "TropicalRainforest"),
	Grassland			UMETA(DisplayName = "Grassland"),
	Woodland			UMETA(DisplayName = "Woodland"),
	SeasonalForest		UMETA(DisplayName = "SeasonalForest"),
	TemperateRainforest UMETA(DisplayName = "TemperateRainforest"),
	BorealForest		UMETA(DisplayName = "BorealForest"),
	Tundra				UMETA(DisplayName = "Tundra"),
	Ice					UMETA(DisplayName = "Ice")
};

static FName BiomeTypeToFName(EBiomeType BiomeType)
{
	return FName(UEnum::GetValueAsString(BiomeType).RightChop(12)); // Removes "EBiomeType::"
}