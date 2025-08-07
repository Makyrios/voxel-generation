// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGen/Enums.h"
#include "BlockSettings.generated.h"

USTRUCT(BlueprintType)
struct VOXELGEN_API FBlockTextureData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Texture")
	int32 TopFaceTexture = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Texture")
	int32 BottomFaceTexture = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Texture")
	int32 SideFaceTexture = 0; // For all other sides

};

UENUM(BlueprintType)
enum class EBlockRenderMode : uint8
{
	Cube UMETA(DisplayName = "Cube"),
	CrossPlanes UMETA(DisplayName = "Cross Planes"),
	CustomMesh UMETA(DisplayName = "Custom Mesh")
};

USTRUCT(BlueprintType)
struct VOXELGEN_API FBlockSettings : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	EBlock BlockID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	EBlockRenderMode RenderMode = EBlockRenderMode::Cube;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	EBlockMaterialType MaterialType = EBlockMaterialType::Opaque;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	bool bIsSolid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	bool bIsTransparent = false; // For visibility culling (e.g., water is transparent, leaves are not fully)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	float RenderScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties")
	float RenderHeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Properties", meta = (EditCondition = "RenderMode == EBlockRenderMode::CrossPlanes"))
	bool RandomRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Texture")
	int NumTextureVariants = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Texture")
	FBlockTextureData TextureData;
	
	// USoundCue* BreakSound;
	// bool bEmitsLight;
	
	FBlockSettings() : BlockID(EBlock::Air) {}
};
