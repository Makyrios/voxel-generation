// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelGenerationGameMode.h"
#include "VoxelGenerationCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVoxelGenerationGameMode::AVoxelGenerationGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
