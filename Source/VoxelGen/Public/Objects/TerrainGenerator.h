// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/BiomeSettingsStruct.h"
#include "VoxelGen/Enums.h"
#include "TerrainGenerator.generated.h"

class UFastNoiseWrapper;

UCLASS()
class VOXELGEN_API UTerrainGenerator : public UActorComponent
{
	GENERATED_BODY()
	
public:
	TArray<EBlock> GenerateTerrain(const FVector& ChunkPosition) const;

	void SetBiomeByName(FName BiomeName);

private:
	float GetHeight(float x, float y) const;

	void InitializeOctaves(const FBiomeSettingsStruct& BiomeSettings);
	
public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<UDataTable> BiomesTable;


private:
	UPROPERTY()
	TArray<UFastNoiseWrapper*> OctaveNoises;

	UPROPERTY()
	FBiomeSettingsStruct CurrentBiomeSettings;
};
