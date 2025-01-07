// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/TerrainGenerator.h"

#include "FastNoiseWrapper.h"
#include "Structs/ChunkData.h"

UTerrainGenerator::UTerrainGenerator()
{
}

void UTerrainGenerator::BeginPlay()
{
	Super::BeginPlay();

	OctaveNoises.SetNum(OctaveSettings.Num());
	for (int i = 0; i < OctaveSettings.Num(); ++i)
	{
		OctaveNoises[i] = NewObject<UFastNoiseWrapper>();
		OctaveNoises[i]->SetupFastNoise(OctaveSettings[i].NoiseType, OctaveSettings[i].Seed, OctaveSettings[i].Frequency);
	}
}

TArray<EBlock> UTerrainGenerator::GenerateTerrain(const FVector& ChunkPosition) const
{
	TArray<EBlock> Blocks;
	
	Blocks.SetNum(FChunkData::ChunkSize * FChunkData::ChunkSize * FChunkData::ChunkSize);
	
	for (int x = 0; x < FChunkData::ChunkSize; ++x)
	{
		for (int y = 0; y < FChunkData::ChunkSize; ++y)
		{
			const float XPos = (x * FChunkData::BlockScaledSize + ChunkPosition.X);
			const float YPos = (y * FChunkData::BlockScaledSize + ChunkPosition.Y);

			// Get the height of the block at the current x and y position by getting the noise value at the x and y position and then scaling it to the chunk height
			const int Height = GetHeight(XPos, YPos);
			
			for (int z = 0; z < FChunkData::ChunkSize; ++z)
			{
				EBlock BlockType = EBlock::Air;
				// Chunk layers:
				// Air
				// Grass
				// Dirt
				// Stone
				if (z < Height - 3)
				{
					BlockType = EBlock::Stone;
				}
				else if (z < Height - 1)
				{
					BlockType = EBlock::Dirt;
				}
				else if (z == Height - 1)
				{
					BlockType = EBlock::Grass;
				}
				
				const int Index = x + (y * FChunkData::ChunkSize) + (z * FChunkData::ChunkSize * FChunkData::ChunkSize);
				Blocks[Index] = BlockType;
			}
		}
	}
	
	return Blocks;
}

float UTerrainGenerator::GetHeight(float x, float y) const
{
	float Height = BaseHeight;
	
	for (int i = 0; i < OctaveNoises.Num(); ++i)
	{
		float Noise = OctaveNoises[i]->GetNoise2D(x / 100.f, y / 100.f);
		Height += Noise * OctaveSettings[i].Amplitude / 2.f;
	}
	
	return FMath::Clamp(Height, 0, FChunkData::ChunkSize);
}