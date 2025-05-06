// Fill out your copyright notice in the Description page of Project Settings.


#include "Structs/ChunkColumn.h"

#include "Structs/BiomeSettings.h"

void FChunkColumn::SetTemperatureData(float NewTemperature)
{
	Temperature = NewTemperature;

	if (NewTemperature < 1.0f / 6.0f)
		TemperatureType = ETemperatureType::Coldest;
	else if (NewTemperature < 2.0f / 6.0f)
		TemperatureType = ETemperatureType::Colder;
	else if (NewTemperature < 3.0f / 6.0f)
		TemperatureType = ETemperatureType::Cold;
	else if (NewTemperature < 4.0f / 6.0f)
		TemperatureType = ETemperatureType::Warm;
	else if (NewTemperature < 5.0f / 6.0f)
		TemperatureType = ETemperatureType::Warmer;
	else
		TemperatureType = ETemperatureType::Warmest;
}

void FChunkColumn::SetHumidityData(float NewHumidity)
{
	Humidity = NewHumidity;

	if (NewHumidity < 1.0f / 6.0f)
		HumidityType = EHumidityType::Dryest;
	else if (NewHumidity < 2.0f / 6.0f)
		HumidityType = EHumidityType::Dryer;
	else if (NewHumidity < 3.0f / 6.0f)
		HumidityType = EHumidityType::Dry;
	else if (NewHumidity < 4.0f / 6.0f)
		HumidityType = EHumidityType::Wet;
	else if (NewHumidity < 5.0f / 6.0f)
		HumidityType = EHumidityType::Wetter;
	else
		HumidityType = EHumidityType::Wettest;
}