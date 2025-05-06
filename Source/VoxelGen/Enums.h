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
	Water UMETA(DisplayName = "Water")
};

UENUM(BlueprintType)
enum class ETemperatureType : uint8
{
	Coldest = 0	UMETA(DisplayName = "Coldest"),
	Colder = 1	UMETA(DisplayName = "Colder"),
	Cold = 2	UMETA(DisplayName = "Cold"),
	Warm = 3	UMETA(DisplayName = "Warm"),
	Warmer = 4	UMETA(DisplayName = "Warmer"),
	Warmest = 5	UMETA(DisplayName = "Warmest"),
};

UENUM(BlueprintType)
enum class EHumidityType : uint8
{
	Dryest = 0	UMETA(DisplayName = "Dryest"),
	Dryer = 1	UMETA(DisplayName = "Dryer"),
	Dry = 2		UMETA(DisplayName = "Dry"),
	Wet = 3		UMETA(DisplayName = "Wet"),
	Wetter = 4	UMETA(DisplayName = "Wetter"),
	Wettest = 5	UMETA(DisplayName = "Wettest"),
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