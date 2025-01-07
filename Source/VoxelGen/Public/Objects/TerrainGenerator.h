// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/NoiseOctaveSettings.h"
#include "VoxelGen/Enums.h"
#include "TerrainGenerator.generated.h"

class UFastNoiseWrapper;

UCLASS()
class VOXELGEN_API UTerrainGenerator : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UTerrainGenerator();

	void BeginPlay() override;
	
	TArray<EBlock> GenerateTerrain(const FVector& ChunkPosition) const;

private:
	float GetHeight(float x, float y) const;
	
public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FNoiseOctaveSettings> OctaveSettings;

	UPROPERTY(EditAnywhere, Category = "Settings")
	int BaseHeight = 8;

private:
	UPROPERTY()
	TArray<UFastNoiseWrapper*> OctaveNoises;
	
};
