// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGen/Enums.h"
#include "BlockLayer.generated.h"

USTRUCT()
struct FBlockLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EBlock BlockType;

	UPROPERTY(EditAnywhere)
	int LayerThickness;
};
