// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/TerrainGenerator.h"

#include "FastNoiseWrapper.h"
#include "Structs/BlockLayer.h"
#include "Structs/ChunkData.h"

TArray<EBlock> UTerrainGenerator::GenerateTerrain(const FVector& ChunkPosition) const
{
	TArray<EBlock> Blocks;
	
	Blocks.SetNum(FChunkData::ChunkSize * FChunkData::ChunkSize * FChunkData::ChunkHeight);
	
	for (int x = 0; x < FChunkData::ChunkSize; ++x)
	{
		for (int y = 0; y < FChunkData::ChunkSize; ++y)
		{
			const float XPos = (x * FChunkData::BlockScaledSize + ChunkPosition.X);
			const float YPos = (y * FChunkData::BlockScaledSize + ChunkPosition.Y);

			const int Height = GetHeight(XPos, YPos);
			int CurrentHeight = Height;
			
			for (const FBlockLayer& Layer : CurrentBiomeSettings.Layers)
			{
				int LayerEndHeight = CurrentHeight - Layer.LayerThickness;
				for (int z = CurrentHeight; z >= 0 && z > LayerEndHeight; --z)
				{
					Blocks[FChunkData::GetBlockIndex(x, y, z)] = Layer.BlockType;
				}
				CurrentHeight = LayerEndHeight;
			}
		}
	}
	
	return Blocks;
}

float UTerrainGenerator::GetHeight(float x, float y) const
{
	float FinalHeight = CurrentBiomeSettings.BaseHeight;

	for (int i = 0; i < OctaveNoises.Num(); ++i)
	{
		if (OctaveNoises[i])
		{
			float NoiseValue = OctaveNoises[i]->GetNoise2D(x / FChunkData::BlockSize, y / FChunkData::BlockSize);
			FinalHeight += NoiseValue * CurrentBiomeSettings.OctaveSettings[i].Amplitude;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Octave Noise at index %d is nullptr."), i);
		}
	}

	return FMath::Clamp(FinalHeight, 0, FChunkData::ChunkHeight);
}

void UTerrainGenerator::SetBiomeByName(FName BiomeName)
{
	if (!BiomesTable) return;

	FBiomeSettingsStruct* FoundBiome = BiomesTable->FindRow<FBiomeSettingsStruct>(BiomeName, TEXT("Lookup Biome"));
	if (FoundBiome)
	{
		CurrentBiomeSettings = *FoundBiome;
		InitializeOctaves(CurrentBiomeSettings);
	}
}

void UTerrainGenerator::InitializeOctaves(const FBiomeSettingsStruct& BiomeSettings)
{
	OctaveNoises.Empty();
	OctaveNoises.SetNum(BiomeSettings.OctaveSettings.Num());

	for (int i = 0; i < BiomeSettings.OctaveSettings.Num(); ++i)
	{
		OctaveNoises[i] = NewObject<UFastNoiseWrapper>();
		OctaveNoises[i]->SetupFastNoise(
			BiomeSettings.OctaveSettings[i].NoiseType,
			BiomeSettings.OctaveSettings[i].Seed,
			BiomeSettings.OctaveSettings[i].Frequency,
			EFastNoise_Interp::Quintic,
			EFastNoise_FractalType::FBM
		);
	}
}