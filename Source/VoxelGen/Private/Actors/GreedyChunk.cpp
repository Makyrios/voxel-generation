// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/GreedyChunk.h"

#include "VoxelGen/Enums.h"

void AGreedyChunk::GenerateMesh()
{
    if (!GetWorld()) return;
    
    // Iterate over each axis (X, Y, Z)
    for (int Axis = 0; Axis < 3; ++Axis)
    {
        // Determine the other two axes
        const int Axis1 = (Axis + 1) % 3;
        const int Axis2 = (Axis + 2) % 3;

        // Initialize delta vectors for axis increments
        auto DeltaAxis1 = FIntVector::ZeroValue;
        auto DeltaAxis2 = FIntVector::ZeroValue;

        // Initialize iterator for chunk traversal and axis mask
        auto ChunkItr = FIntVector::ZeroValue;
        auto AxisMask = FIntVector::ZeroValue;

        // Set the mask to target the current axis
        AxisMask[Axis] = 1;

        // Create a mask array to hold visibility and block data
        TArray<FMask> Mask;
        const int Size = FChunkData::GetChunkSize(GetWorld());
        const int ChunkHeight = FChunkData::GetChunkHeight(GetWorld()); // Height of the chunk

        int InnerAxisSize1; // Width of the slice (along Axis1)
        int InnerAxisSize2; // Height of the slice (along Axis2)

        if (Axis == 2) { // Z-axis (Top/Bottom)
            InnerAxisSize1 = Size;
            InnerAxisSize2 = Size;
        } else if (Axis == 0) { // X-axis (Walls along X)
            InnerAxisSize1 = Size;
            InnerAxisSize2 = ChunkHeight;
        } else { // Axis == 1 (Y-axis) (Walls along Y)
            InnerAxisSize1 = ChunkHeight;
            InnerAxisSize2 = Size;
        }
        Mask.SetNum(InnerAxisSize1 * InnerAxisSize2);

        // Determine if a block is "Solid Opaque"
        auto IsSolidOpaque = [&](EBlock BlockType, const FBlockSettings& Settings) -> bool {
            if (BlockType == EBlock::Air) return false;
            return Settings.bIsSolid && !Settings.bIsTransparent;
        };

        // Sweep along the current axis
        int MainAxisSize = (Axis == 2) ? ChunkHeight : Size; // Determine the size along the current axis
        for (ChunkItr[Axis] = -1; ChunkItr[Axis] < MainAxisSize;)
        {
            int N = 0;

            // Traverse along Axis2 (e.g., Y) - Height of the slice
            for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < InnerAxisSize2; ++ChunkItr[Axis2])
            {
                // Traverse along Axis1 (e.g., X) - Width of the slice
                for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < InnerAxisSize1; ++ChunkItr[Axis1])
                {
                    const EBlock CurrentBlockType = GetBlockAtPosition(ChunkItr);
                    FBlockSettings CurrentSettings = GetBlockData(CurrentBlockType);

                    EBlock CompareBlockType = GetBlockAtPosition(ChunkItr + AxisMask);
                    FBlockSettings CompareSettings = GetBlockData(CompareBlockType);

                    bool bCurrentIsSolidOpaque = IsSolidOpaque(CurrentBlockType, CurrentSettings);
                    bool bCompareIsSolidOpaque = IsSolidOpaque(CompareBlockType, CompareSettings);

                    FBlockSettings DefaultSettings;

                    FMask ResultMask = {DefaultSettings, 0}; // Default to no face

                    if (bCurrentIsSolidOpaque == bCompareIsSolidOpaque)
                    {
                        // Both are SolidOpaque OR Neither is SolidOpaque
                        if (!bCurrentIsSolidOpaque) // Case: neither is SolidOpaque. (Air, Transparent, Masked/NonSolid)
                        {
                            bool bCurrentIsAir = (CurrentBlockType == EBlock::Air);
                            bool bCompareIsAir = (CompareBlockType == EBlock::Air);

                            bool bCurrentIsTransparent = CurrentSettings.bIsTransparent;
                            bool bCompareIsTransparent = CompareSettings.bIsTransparent;

                            // Rule 1: Transparent vs. Same Transparent (e.g. Water vs Water) -> No face
                            if (bCurrentIsTransparent && bCompareIsTransparent && CurrentBlockType == CompareBlockType)
                            {
                                // ResultMask remains {EBlock::Air, 0}
                            }
                            // Rule 2: Current is (Transparent or Masked/NonSolid) AND Compare is Air
                            else if (!bCurrentIsAir && bCompareIsAir)
                            {
                                ResultMask = {CurrentSettings, 1};
                            }
                            // Rule 3: Current is Air AND Compare is (Transparent or Masked/NonSolid)
                            else if (bCurrentIsAir && !bCompareIsAir)
                            {
                                ResultMask = {CompareSettings, -1};
                            }
                            // Rule 4: Both are (Transparent or Masked/NonSolid) but not Air, and not same-type transparent.
                            // (Water vs Glass, Water vs Leaves, Leaves vs Flowers)
                            // Greedy mesher picks one. Prioritize CurrentBlock for normal = 1.
                            else if (!bCurrentIsAir && !bCompareIsAir)
                            {
                                ResultMask = {CurrentSettings, 1};
                            }
                        }
                    }
                    else // One is SolidOpaque, the other is not
                    {
                        if (bCurrentIsSolidOpaque) // Current is SolidOpaque, Compare is (Air, Transparent, or Masked/NonSolid)
                        {
                            ResultMask = {CurrentSettings, 1}; // Stone vs Water, Stone vs Air
                        }
                        else // Current is (Air, Transparent, or Masked/NonSolid), Compare is SolidOpaque
                        {
                            ResultMask = {CompareSettings, -1}; // Water vs Stone, Air vs Stone
                        }
                    }
                    
                    Mask[N++] = ResultMask;
                }
            }

            // Move to the next slice along the current axis
            ++ChunkItr[Axis];
            N = 0; // Reset mask index for processing

            // Iterate over the mask to generate quads for visible faces
            for (int j = 0; j < InnerAxisSize2; ++j) // Iterate through height of the mask
            {
                for (int i = 0; i < InnerAxisSize1;) // Iterate through width of the mask
                {
                    // If there is a visible face at this mask position
                    if (Mask[N].Normal != 0)
                    {
                        const auto CurrentMaskValue = Mask[N];
                        
                        // Temporarily set ChunkItr to the start of the potential quad in the current slice
                        // ChunkItr[Axis] is already set to the current slice's depth.
                        // We need to set ChunkItr[Axis1] and ChunkItr[Axis2] based on i and j.
                        FIntVector QuadStartPos = ChunkItr;
                        QuadStartPos[Axis1] = i;
                        QuadStartPos[Axis2] = j;


                        // Determine the width of the quad
                        int Width;
                        for (Width = 1; i + Width < InnerAxisSize1 && CompareMask(Mask[N + Width], CurrentMaskValue); ++Width) {}

                        // Determine the height of the quad
                        int Height;
                        bool Done = false;

                        for (Height = 1; j + Height < InnerAxisSize2; ++Height)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                if (!CompareMask(Mask[N + k + Height * InnerAxisSize1], CurrentMaskValue))
                                {
                                    Done = true;
                                    break;
                                }
                            }
                            if (Done) break;
                        }
                        
                        DeltaAxis1[Axis1] = Width; 
                        DeltaAxis2[Axis2] = Height;

                        if (CurrentMaskValue.BlockProperties.RenderMode == EBlockRenderMode::Cube)
                        {
                            CreateQuad(
                                CurrentMaskValue, AxisMask, Width, Height,
                                QuadStartPos,
                                QuadStartPos + DeltaAxis1,
                                QuadStartPos + DeltaAxis2,
                                QuadStartPos + DeltaAxis1 + DeltaAxis2
                            );
                        }

                        // Create a quad for the current visible face

                        // Reset delta values for next quad
                        DeltaAxis1 = FIntVector::ZeroValue;
                        DeltaAxis2 = FIntVector::ZeroValue;


                        // Mark the area covered by the quad as processed
                        for (int l = 0; l < Height; ++l)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                Mask[N + k + l * InnerAxisSize1] = FMask{FBlockSettings(), 0};
                            }
                        }

                        // Skip to the end of the quad in the mask
                        i += Width;
                        N += Width;
                    }
                    else
                    {
                        // Move to the next position if no visible face
                        ++i;
                        ++N;
                    }
                }
            }
        }
    }

    const int32 ChunkSize   = FChunkData::GetChunkSize(GetWorld());
    const int32 ChunkHeight = FChunkData::GetChunkHeight(GetWorld());

    for (int x = 0; x < ChunkSize; ++x)
    {
        for (int y = 0; y < ChunkSize; ++y)
        {
            for (int z = 0; z < ChunkHeight; ++z)
            {
                FIntVector Pos(x,y,z);
                EBlock BlockType = GetBlockAtPosition(Pos);
                FBlockSettings Settings = GetBlockData(BlockType);

                if (Settings.RenderMode != EBlockRenderMode::CrossPlanes)
                    continue;

                CreateCrossPlanes(Pos, BlockType, Settings);
            }
        }
    }
}

void AGreedyChunk::CreateQuad(
    const FMask& Mask,
    const FIntVector& AxisMask,
    const int Width,
    const int Height,
    const FIntVector& V1,
    const FIntVector& V2,
    const FIntVector& V3,
    const FIntVector& V4
)
{
    if (!GetWorld()) return;
    
    const auto Normal = FVector(AxisMask * Mask.Normal);
    const auto Color = FColor(0, 0, 0, GetTextureIndex(Mask.BlockProperties.BlockID, Normal));
    
    FChunkMeshData& TargetMeshData = GetMeshDataForBlock(Mask.BlockProperties.BlockID);
    int& TargetVertexCount = GetVertexCountForBlock(Mask.BlockProperties.BlockID);

    TargetMeshData.Vertices.Append({
        FVector(V1) * FChunkData::GetScaledBlockSize(GetWorld()),
        FVector(V2) * FChunkData::GetScaledBlockSize(GetWorld()),
        FVector(V3) * FChunkData::GetScaledBlockSize(GetWorld()),
        FVector(V4) * FChunkData::GetScaledBlockSize(GetWorld())
        });

    TargetMeshData.Triangles.Append({
        TargetVertexCount,
        TargetVertexCount + 2 + Mask.Normal,
        TargetVertexCount + 2 - Mask.Normal,
        TargetVertexCount + 3,
        TargetVertexCount + 1 - Mask.Normal,
        TargetVertexCount + 1 + Mask.Normal
        });

    TargetMeshData.Normals.Append({ Normal, Normal, Normal, Normal });

    TargetMeshData.Colors.Append({ Color, Color, Color, Color });

    if (Normal.X == 1 || Normal.X == -1)
    {
        TargetMeshData.UV.Append({
            FVector2D(Width, Height),
            FVector2D(0, Height),
            FVector2D(Width, 0),
            FVector2D(0, 0),
            });
    }
    else
    {
        TargetMeshData.UV.Append({
            FVector2D(Height, Width),
            FVector2D(Height, 0),
            FVector2D(0, Width),
            FVector2D(0, 0),
            });
    }

    TargetVertexCount += 4;
}

bool AGreedyChunk::CompareMask(const FMask& M1, const FMask& M2) const
{
    return M1.BlockProperties.BlockID == M2.BlockProperties.BlockID && M1.Normal == M2.Normal;
}