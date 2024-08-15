// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VoxelGenerationPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class VOXELGENERATION_API AVoxelGenerationPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

protected:
	virtual void BeginPlay() override;
};
