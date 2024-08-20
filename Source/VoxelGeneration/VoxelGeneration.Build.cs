// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VoxelGeneration : ModuleRules
{
	public VoxelGeneration(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "FastNoise", "FastNoiseGenerator" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });
	}
}
