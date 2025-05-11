// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/FoliageGenerator.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Structs/BiomeSettings.h"
#include "Structs/ChunkColumn.h"

UFoliageGenerator::UFoliageGenerator()
{
}

int GetColumnIndex(int LocalX, int LocalY, int ChunkSize)
{
    return LocalX + (LocalY * ChunkSize);
}

bool UFoliageGenerator::AttemptPlaceFoliageAt(TArray<FChunkColumn>& ChunkColumnsData, int LocalX, int LocalY,
    const FBiomeSettings* BiomeInfo, const FRandomStream& ColumnSpecificStream, int ChunkSize, int ChunkHeight)
{
    if (!BiomeInfo) return false;

    int ColumnIndex = GetColumnIndex(LocalX, LocalY, ChunkSize);
    if (ColumnIndex < 0 || ColumnIndex >= ChunkColumnsData.Num()) return false;
    
    FChunkColumn& CurrentColumn = ChunkColumnsData[ColumnIndex];

    int TopSolidZ = CurrentColumn.Height;
    if (TopSolidZ < 0 || TopSolidZ >= ChunkHeight - 1) return false;

    EBlock SurfaceBlock = CurrentColumn.Blocks[TopSolidZ];
    int SpawnZ = TopSolidZ + 1;

    // Try to place major foliage (trees, cactus)
    for (const FFoliageSpawnRule& Rule : BiomeInfo->FoliageRules)
    {
        if (!Rule.AllowedSurfaceBlocks.Contains(SurfaceBlock)) continue;
        
        if (ColumnSpecificStream.FRand() < Rule.SpawnChancePerColumn)
        {
             // Check if spawn spot in this column is clear
            if (SpawnZ < ChunkHeight && CurrentColumn.Blocks[SpawnZ] == EBlock::Air)
            {
                // Create a new stream for the specific tree's internal variations
                FRandomStream FoliageInstanceStream;
                // Mix in type for variety
                FoliageInstanceStream.Initialize(ColumnSpecificStream.GetCurrentSeed() ^ (int32)Rule.Type); 

                int FoliageHeight = FoliageInstanceStream.RandRange(Rule.MinHeight, Rule.MaxHeight);
                bool bIsVariant = FoliageInstanceStream.FRand() < Rule.VariantChance;
                
                FIntVector FoliageBaseLocalPos(LocalX, LocalY, SpawnZ);

                // Basic vertical fit check
                if (SpawnZ + FoliageHeight + 3 >= ChunkHeight) {
                    FoliageHeight = ChunkHeight - SpawnZ - 4;
                    if (FoliageHeight < Rule.MinHeight && Rule.Type != EFoliageType::GrassPlant) continue;
                }
                if (FoliageHeight < 1 && Rule.Type == EFoliageType::GrassPlant) continue;


                switch (Rule.Type)
                {
                    case EFoliageType::OakTree:
                        GenerateOakTree(ChunkColumnsData, FoliageBaseLocalPos, FoliageHeight, bIsVariant, FoliageInstanceStream, ChunkSize, ChunkHeight);
                        return true;
                    case EFoliageType::BirchTree:
                        GenerateBirchTree(ChunkColumnsData, FoliageBaseLocalPos, FoliageHeight, bIsVariant, FoliageInstanceStream, ChunkSize, ChunkHeight);
                        return true;
                    case EFoliageType::Cactus:
                        GenerateCactus(ChunkColumnsData, FoliageBaseLocalPos, FoliageHeight, FoliageInstanceStream, ChunkSize, ChunkHeight);
                        return true;
                    case EFoliageType::GrassPlant:
                        break;
                }
            }
        }
        
    }

    // If no major foliage was placed, try to place surface grass
    if (BiomeInfo->SurfaceGrassChance > 0 && BiomeInfo->GrassSpawnableOn.Contains(SurfaceBlock))
    {
        if (ColumnSpecificStream.FRand() < BiomeInfo->SurfaceGrassChance)
        {
            if (SpawnZ < ChunkHeight && CurrentColumn.Blocks[SpawnZ] == EBlock::Air)
            {
                GenerateGrass(CurrentColumn.Blocks, SpawnZ, ChunkHeight);
            }
        }
    }
    // No major foliage placed
    return false;
}

void UFoliageGenerator::GenerateOakTree(TArray<FChunkColumn>& ChunkColumnsData, const FIntVector& TreeBaseLocalPosInChunk,
    int FoliageHeight, bool bLargeVariant, FRandomStream& Stream, int ChunkSize, int ChunkHeight)
{
    int TrunkX = TreeBaseLocalPosInChunk.X;
    int TrunkY = TreeBaseLocalPosInChunk.Y;
    int BaseZ = TreeBaseLocalPosInChunk.Z;

    // Ensure tree + some canopy space fits.
    if (BaseZ + FoliageHeight + 2 >= ChunkHeight || FoliageHeight < 3) return;

    // Trunk
    for (int i = 0; i < FoliageHeight; ++i)
    {
        SetBlockInChunkColumns(ChunkColumnsData, TrunkX, TrunkY, BaseZ + i, EBlock::OakLog, ChunkSize, ChunkHeight);
    }

    // Canopy (More spherical/rounded)
    int CanopyRadius = bLargeVariant ? 3 : 2;
    // How many layers up/down from center of canopy mass
    int CanopyVExtent = bLargeVariant ? 2 : 2;
    // Center the mass a bit below very top of trunk
    int CanopyCenterZ = BaseZ + FoliageHeight - CanopyVExtent + Stream.RandRange(-1,0);
    
    for (int RelZ = -CanopyVExtent; RelZ <= CanopyVExtent +1; ++RelZ)
    {
        int CurrentLeafZ = CanopyCenterZ + RelZ;
        // Don't let leaves go too low
        if (CurrentLeafZ < BaseZ + 1) continue;

        // Determine horizontal radius for this Z layer (wider in middle)
        float LayerProgress = FMath::Abs((float)RelZ / (CanopyVExtent + 0.5f)); // 0 at center, 1 at edges
        int CurrentLayerHRadius = FMath::RoundToInt(CanopyRadius * FMath::Sqrt(1.0f - FMath::Square(LayerProgress)));
        
        // Make top and bottom layers a bit less dense/wide
        if (FMath::Abs(RelZ) == CanopyVExtent) CurrentLayerHRadius = FMath::Max(0, CurrentLayerHRadius -1);
        if (RelZ == CanopyVExtent + 1) CurrentLayerHRadius = FMath::Max(0, CurrentLayerHRadius-1); // Fluff on top


        for (int RelX = -CurrentLayerHRadius; RelX <= CurrentLayerHRadius; ++RelX)
        {
            for (int RelY = -CurrentLayerHRadius; RelY <= CurrentLayerHRadius; ++RelY)
            {
                // Check if within circular radius for this layer
                float DistSq = RelX * RelX + RelY * RelY;
                if (DistSq <= FMath::Square(CurrentLayerHRadius + 0.5f)) // +0.5f for rounder feel
                {
                    // Avoid placing leaves directly inside the trunk for upper canopy parts unless it's the base of the canopy
                    if (CurrentLeafZ >= BaseZ + FoliageHeight -1 && RelX == 0 && RelY == 0 && CurrentLayerHRadius > 0)
                    {
                        // If it's the center point above the trunk, ensure it's leaves
                         SetBlockInChunkColumns(ChunkColumnsData, TrunkX + RelX, TrunkY + RelY, CurrentLeafZ, EBlock::OakLeaves, ChunkSize, ChunkHeight);
                        continue;
                    }
                    
                    // Random chance to skip some inner blocks for a less solid look
                    if (DistSq < FMath::Square(CurrentLayerHRadius - 0.5f) && Stream.FRand() < 0.2f && CurrentLayerHRadius > 1) continue;

                    SetBlockInChunkColumns(ChunkColumnsData, TrunkX + RelX, TrunkY + RelY, CurrentLeafZ, EBlock::OakLeaves, ChunkSize, ChunkHeight);
                }
            }
        }
    }
}

void UFoliageGenerator::GenerateBirchTree(TArray<FChunkColumn>& ChunkColumnsData, const FIntVector& TreeBaseLocalPosInChunk,
    int FoliageHeight, bool bLargeVariant, FRandomStream& Stream, int ChunkSize, int ChunkHeight)
{
    int TrunkX = TreeBaseLocalPosInChunk.X;
    int TrunkY = TreeBaseLocalPosInChunk.Y;
    int BaseZ = TreeBaseLocalPosInChunk.Z;

    int ActualHeight = FoliageHeight + Stream.RandRange(0, bLargeVariant ? 2 : 1);
    if (BaseZ + ActualHeight + 2 >= ChunkHeight || ActualHeight < 4) return;

    // Trunk
    for (int i = 0; i < ActualHeight; ++i)
    {
        SetBlockInChunkColumns(ChunkColumnsData, TrunkX, TrunkY, BaseZ + i, EBlock::BirchLog, ChunkSize, ChunkHeight);
    }

    // Birch Canopy (Taller, more "fluffy" or sparse, less perfectly round)
    
    // Vertical extent from near trunk top upwards
    int CanopyVRadius = bLargeVariant ? 4 : 3;
    // Max horizontal spread
    int CanopyHRadiusMax = bLargeVariant ? 2 : 2;

    // Start canopy a bit down from the very top
    int CanopyBaseZ = BaseZ + ActualHeight - Stream.RandRange(1,2);

    for (int RelZ = 0; RelZ <= CanopyVRadius; ++RelZ) 
    {
        int CurrentLeafZ = CanopyBaseZ + RelZ;
        if (CurrentLeafZ < BaseZ + ActualHeight / 2) continue;

        // Determine horizontal radius for this layer
        float LayerProgress = (float)RelZ / (float)CanopyVRadius; // 0 at canopy base, 1 at canopy top
        int LayerHRadius = FMath::CeilToInt(CanopyHRadiusMax * (1.0f - LayerProgress * LayerProgress * 0.7f));
        if (RelZ == 0 && LayerHRadius < 1) LayerHRadius = 1; // Ensure some leaves at base of canopy
        if (RelZ == CanopyVRadius) LayerHRadius = FMath::Max(0, LayerHRadius-1); // Taper top

        for (int RelX = -LayerHRadius; RelX <= LayerHRadius; ++RelX)
        {
            for (int RelY = -LayerHRadius; RelY <= LayerHRadius; ++RelY)
            {
                // Use a slightly more diamond/oval check, or just random skipping
                if (FMath::Abs(RelX) + FMath::Abs(RelY) <= LayerHRadius + Stream.FRandRange(0.0f, 1.0f) ) // More random shape
                {
                    // Skip center if it's not the very top, to avoid dense core right above trunk
                    if (RelZ < CanopyVRadius -1 && RelX == 0 && RelY == 0 && LayerHRadius > 0) continue;

                    // Random chance to make it sparser
                    if (Stream.FRand() < 0.85f) // 85% chance to place a leaf if in range
                    {
                       SetBlockInChunkColumns(ChunkColumnsData, TrunkX + RelX, TrunkY + RelY, CurrentLeafZ, EBlock::BirchLeaves, ChunkSize, ChunkHeight);
                    }
                }
            }
        }
    }
}

void UFoliageGenerator::GenerateCactus(TArray<FChunkColumn>& ChunkColumnsData, const FIntVector& CactusBaseLocalPosInChunk,
    int FoliageHeight, FRandomStream& Stream, int ChunkSize, int ChunkHeight)
{
    int TrunkX = CactusBaseLocalPosInChunk.X;
    int TrunkY = CactusBaseLocalPosInChunk.Y;
    int BaseZ = CactusBaseLocalPosInChunk.Z;

    if (BaseZ + FoliageHeight >= ChunkHeight || FoliageHeight < 1) return;

    // Main Trunk
    for (int i = 0; i < FoliageHeight; ++i)
    {
        SetBlockInChunkColumns(ChunkColumnsData, TrunkX, TrunkY, BaseZ + i, EBlock::CactusBlock, ChunkSize, ChunkHeight);
    }

    // Only add arms if trunk is tall enough
    if (FoliageHeight > 2)
    {
        int NumArms = Stream.RandRange(0, 3);
        for (int ArmCount = 0; ArmCount < NumArms; ++ArmCount)
        {
            // Arm properties
            // Arm starts somewhere along the trunk
            int ArmBaseZ = BaseZ + Stream.RandRange(1, FoliageHeight - 2);
            // Arm height limited by trunk
            int ArmHeight = Stream.RandRange(1, FMath::Min(3, FoliageHeight - (ArmBaseZ - BaseZ) ));
            
            // Determine arm direction (0: +X, 1: -X, 2: +Y, 3: -Y)
            // Ensure arms don't try to spawn on the same side repeatedly for a single cactus
            int ArmDirection = Stream.RandRange(0, 3);

            int ArmX = TrunkX;
            int ArmY = TrunkY;

            // Project one step out for arm base
            if (ArmDirection == 0) ArmX++; else if (ArmDirection == 1) ArmX--;
            else if (ArmDirection == 2) ArmY++; else if (ArmDirection == 3) ArmY--;

            // Check if space for arm base is clear (not another cactus block from trunk or previous arm)
            // This requires checking ChunkColumnsData at (ArmX, ArmY, ArmBaseZ)
            if (ArmX >= 0 && ArmX < ChunkSize && ArmY >= 0 && ArmY < ChunkSize && ArmBaseZ < ChunkHeight) {
                int CheckColumnIndex = GetColumnIndex(ArmX, ArmY, ChunkSize);
                 if (CheckColumnIndex < ChunkColumnsData.Num() && ChunkColumnsData[CheckColumnIndex].Blocks[ArmBaseZ] != EBlock::Air) {
                     // Space for arm base is not clear
                     continue;
                 }
            } else {
                // Arm base out of chunk bounds
                continue;
            }


            for (int h = 0; h < ArmHeight; ++h)
            {
                SetBlockInChunkColumns(ChunkColumnsData, ArmX, ArmY, ArmBaseZ + h, EBlock::CactusBlock, ChunkSize, ChunkHeight);
            }
        }
    }
}

void UFoliageGenerator::GenerateGrass(TArray<EBlock>& Blocks, int BaseZ, int ChunkHeight)
{
    SetBlockInSingleColumnArray(Blocks, BaseZ, EBlock::GrassFoliage, ChunkHeight);
}

void UFoliageGenerator::SetBlockInChunkColumns(TArray<FChunkColumn>& ChunkColumnsData, int LocalX, int LocalY,
    int LocalZ, EBlock BlockType, int ChunkSize, int ChunkHeight) const
{
    if (LocalX >= 0 && LocalX < ChunkSize &&
        LocalY >= 0 && LocalY < ChunkSize &&
        LocalZ >= 0 && LocalZ < ChunkHeight)
    {
        int ColumnIndex = GetColumnIndex(LocalX, LocalY, ChunkSize);

        if (ColumnIndex < 0 || ColumnIndex >= ChunkColumnsData.Num()) return;
        
        if (LocalZ < 0 || LocalZ >= ChunkColumnsData[ColumnIndex].Blocks.Num()) return;

        if (ChunkColumnsData[ColumnIndex].Blocks[LocalZ] == EBlock::Air)
        {
            ChunkColumnsData[ColumnIndex].Blocks[LocalZ] = BlockType;
        }
    }
}

void UFoliageGenerator::SetBlockInSingleColumnArray(TArray<EBlock>& Blocks, int Z, EBlock BlockType, int ChunkHeight) const
{
    if (Z >= 0 && Z < ChunkHeight)
    {
        if (Z >= Blocks.Num()) return;
        
        if (Blocks[Z] == EBlock::Air)
        {
            Blocks[Z] = BlockType;
        }
    }
}
