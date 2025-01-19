// Fill out your copyright notice in the Description page of Project Settings.


#include "Structs/NoiseOctaveSettings.h"

// EBlock FNoiseOctaveSettings::GetBlockBasedOnHeight(int MaxHeight, int CurrentHeight) const
// {
// 	EBlock BlockType = EBlock::Air;
//
// 	// Chunk layers:
// 	// Air
// 	// Grass
// 	// Dirt
// 	// Stone
// 	
// 	if (CurrentHeight < MaxHeight - 3)
// 	{
// 		BlockType = EBlock::Stone;
// 	}
// 	else if (CurrentHeight < MaxHeight - 1)
// 	{
// 		BlockType = EBlock::Dirt;
// 	}
// 	else if (CurrentHeight == MaxHeight - 1)
// 	{
// 		BlockType = EBlock::Grass;
// 	}
// 	else if (CurrentHeight == MaxHeight)
// 	{
// 		BlockType = EBlock::Grass;
// 	}
// 	
// 	return BlockType;
// }
