// Copyright Epic Games, Inc. All Rights Reserved.


#include "Player/PlayerControllers/VoxelGenerationPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

void AVoxelGenerationPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!GetLocalPlayer()) return;
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(InputMappingContext, 0);
	}
}