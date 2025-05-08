// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/GreedyChunk.h"

#include "VoxelGen/Enums.h"

void AGreedyChunk::GenerateMesh()
{
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
        const int Size = FChunkData::GetChunkSize(this);
        const int ChunkHeight = FChunkData::GetChunkHeight(this); // Height of the chunk

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

        // Helper lambda to determine if a block is "Solid Opaque"
        // A block is Solid Opaque if it's not Air, has settings, is solid, and not transparent.
        auto IsSolidOpaque = [&](EBlock BlockType, const FBlockSettings* Settings) -> bool {
            if (BlockType == EBlock::Air) return false;
            if (Settings) {
                return Settings->bIsSolid && !Settings->bIsTransparent;
            }
            // Fallback for blocks with no settings (should ideally not happen for non-Air blocks)
            // Treat as solid opaque by default if not Air and no settings.
            return true; 
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
                    // Original logic for skipping faces on the -1 boundary plane of the CHUNK ITERATION
                    // This means if ChunkItr[Axis] is -1, this entire mask slice will be empty.
                    // This behavior is preserved.
                    if (Axis == 0 && ChunkItr.X == -1) // ChunkItr.X is ChunkItr[Axis] when Axis is 0
                    {
                        Mask[N++] = FMask{EBlock::Air, 0};
                        continue;
                    }
                    if (Axis == 1 && ChunkItr.Y == -1) // ChunkItr.Y is ChunkItr[Axis] when Axis is 1
                    {
                        Mask[N++] = FMask{EBlock::Air, 0};
                        continue;
                    }
                    if (Axis == 2 && ChunkItr.Z == -1) // ChunkItr.Z is ChunkItr[Axis] when Axis is 2
                    {
                        Mask[N++] = FMask{EBlock::Air, 0};
                        continue;
                    }
                    
                    const EBlock CurrentBlockType = GetBlockAtPosition(ChunkItr);
                    const EBlock CompareBlockType = GetBlockAtPosition(ChunkItr + AxisMask);

                    const FBlockSettings* CurrentSettings = GetBlockData(CurrentBlockType);
                    const FBlockSettings* CompareSettings = GetBlockData(CompareBlockType);

                    bool bCurrentIsSolidOpaque = IsSolidOpaque(CurrentBlockType, CurrentSettings);
                    bool bCompareIsSolidOpaque = IsSolidOpaque(CompareBlockType, CompareSettings);

                    FMask ResultMask = {EBlock::Air, 0}; // Default to no face

                    if (bCurrentIsSolidOpaque == bCompareIsSolidOpaque)
                    {
                        // Both are SolidOpaque OR Neither is SolidOpaque
                        if (!bCurrentIsSolidOpaque) // Case: NEITHER is SolidOpaque. (e.g., Air, Transparent, Masked/NonSolid)
                        {
                            bool bCurrentIsAir = (CurrentBlockType == EBlock::Air);
                            bool bCompareIsAir = (CompareBlockType == EBlock::Air);

                            bool bCurrentIsTransparent = CurrentSettings && CurrentSettings->bIsTransparent;
                            bool bCompareIsTransparent = CompareSettings && CompareSettings->bIsTransparent;

                            // Rule 1: Transparent vs. Same Transparent (e.g. Water vs Water) -> No face
                            if (bCurrentIsTransparent && bCompareIsTransparent && CurrentBlockType == CompareBlockType)
                            {
                                // No face needed, ResultMask remains {EBlock::Air, 0}
                            }
                            // Rule 2: Current is (Transparent or Masked/NonSolid) AND Compare is Air
                            else if (!bCurrentIsAir && bCompareIsAir)
                            {
                                ResultMask = {CurrentBlockType, 1};
                            }
                            // Rule 3: Current is Air AND Compare is (Transparent or Masked/NonSolid)
                            else if (bCurrentIsAir && !bCompareIsAir)
                            {
                                ResultMask = {CompareBlockType, -1};
                            }
                            // Rule 4: Both are (Transparent or Masked/NonSolid) but not Air, and not same-type transparent.
                            // (e.g. Water vs Glass, Water vs Leaves, Leaves vs Flowers)
                            // Greedy mesher picks one. Prioritize CurrentBlock for normal = 1.
                            else if (!bCurrentIsAir && !bCompareIsAir)
                            {
                                ResultMask = {CurrentBlockType, 1};
                            }
                            // else (e.g. Air vs Air), ResultMask remains {EBlock::Air, 0}
                        }
                        // else (Both ARE SolidOpaque, e.g. Stone vs Granite), ResultMask remains {EBlock::Air, 0}
                    }
                    else // One is SolidOpaque, the other is not
                    {
                        if (bCurrentIsSolidOpaque) // Current is SolidOpaque, Compare is (Air, Transparent, or Masked/NonSolid)
                        {
                            ResultMask = {CurrentBlockType, 1}; // e.g. Stone vs Water, Stone vs Air
                        }
                        else // Current is (Air, Transparent, or Masked/NonSolid), Compare is SolidOpaque
                        {
                            ResultMask = {CompareBlockType, -1}; // e.g. Water vs Stone, Air vs Stone
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
                        const auto CurrentMaskValue = Mask[N]; // Renamed from CurrentMask to avoid conflict
                        
                        // Temporarily set ChunkItr to the start of the potential quad in the current slice
                        // ChunkItr[Axis] is already set to the current slice's depth.
                        // We need to set ChunkItr[Axis1] and ChunkItr[Axis2] based on i and j.
                        FIntVector QuadStartPos = ChunkItr; // ChunkItr currently holds slice position, e.g., (depth, 0, 0)
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
                        
                        DeltaAxis1[Axis1] = Width; // No longer ZeroValue from start of Axis loop
                        DeltaAxis2[Axis2] = Height; // No longer ZeroValue

                        // Create a quad for the current visible face
                        CreateQuad(
                            CurrentMaskValue, AxisMask, Width, Height,
                            QuadStartPos,
                            QuadStartPos + DeltaAxis1,
                            QuadStartPos + DeltaAxis2,
                            QuadStartPos + DeltaAxis1 + DeltaAxis2
                        );

                        // Reset delta values for next quad
                        DeltaAxis1 = FIntVector::ZeroValue;
                        DeltaAxis2 = FIntVector::ZeroValue;


                        // Mark the area covered by the quad as processed
                        for (int l = 0; l < Height; ++l)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                Mask[N + k + l * InnerAxisSize1] = FMask{EBlock::Air, 0};
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
    const auto Normal = FVector(AxisMask * Mask.Normal);
    const auto Color = FColor(0, 0, 0, GetTextureIndex(Mask.Block, Normal));
    
    FChunkMeshData& TargetMeshData = GetMeshDataForBlock(Mask.Block);
    int& TargetVertexCount = GetVertexCountForBlock(Mask.Block);

    TargetMeshData.Vertices.Append({
        FVector(V1) * FChunkData::GetScaledBlockSize(this),
        FVector(V2) * FChunkData::GetScaledBlockSize(this),
        FVector(V3) * FChunkData::GetScaledBlockSize(this),
        FVector(V4) * FChunkData::GetScaledBlockSize(this)
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
    return M1.Block == M2.Block && M1.Normal == M2.Normal;
}


// Fill out your copyright notice in the Description page of Project Settings.
//
//
// #include "Actors/GreedyChunk.h"
// #include "VoxelGen/Enums.h"
// #include "Structs/BlockSettings.h"
//
// void AGreedyChunk::GenerateMesh()
// {
//     // Iterate over each axis (X, Y, Z)
//     for (int Axis = 0; Axis < 3; ++Axis)
//     {
//         const int Axis1 = (Axis + 1) % 3;
//         const int Axis2 = (Axis + 2) % 3;
//
//         FIntVector DeltaAxis1 = FIntVector::ZeroValue;
//         FIntVector DeltaAxis2 = FIntVector::ZeroValue;
//         FIntVector ChunkItr = FIntVector::ZeroValue;
//         FIntVector AxisMask = FIntVector::ZeroValue;
//         AxisMask[Axis] = 1;
//
//         TArray<FMask> Mask;
//         const int Size = FChunkData::GetChunkSize(this);
//         const int ChunkHeight = FChunkData::GetChunkHeight(this);
//         int InnerAxisSize1 = (Axis == 1) ? ChunkHeight : Size;
//         int InnerAxisSize2 = (Axis == 2) ? Size : ((Axis == 0) ? ChunkHeight : Size);
//         Mask.SetNum(InnerAxisSize1 * InnerAxisSize2);
//
//         int MainAxisSize = (Axis == 2) ? ChunkHeight : Size;
//         for (ChunkItr[Axis] = -1; ChunkItr[Axis] < MainAxisSize; )
//         {
//             int N = 0;
//             // Build visibility mask for this slice
//             for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < InnerAxisSize2; ++ChunkItr[Axis2])
//             {
//                 for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < InnerAxisSize1; ++ChunkItr[Axis1])
//                 {
//                     // Determine current and neighbor block
//                     EBlock CurrBlock = GetBlockAtPosition(ChunkItr);
//                     EBlock NeighBlock = GetBlockAtPosition(ChunkItr + AxisMask);
//
//                     // Only air vs non-air boundaries produce faces
//                     const bool CurrAir = (CurrBlock == EBlock::Air);
//                     const bool NeighAir = (NeighBlock == EBlock::Air);
//
//                     if (CurrAir == NeighAir)
//                     {
//                         Mask[N++] = FMask{EBlock::Air, 0};
//                     }
//                     else if (!CurrAir)
//                     {
//                         Mask[N++] = FMask{CurrBlock, 1};
//                     }
//                     else
//                     {
//                         Mask[N++] = FMask{NeighBlock, -1};
//                     }
//                 }
//             }
//
//             // Advance slice
//             ++ChunkItr[Axis];
//             N = 0;
//
//             // Greedy quad generation
//             for (int j = 0; j < InnerAxisSize2; ++j)
//             {
//                 for (int i = 0; i < InnerAxisSize1; )
//                 {
//                     const FMask Cell = Mask[N];
//                     if (Cell.Normal != 0)
//                     {
//                         // Start mask run
//                         ChunkItr[Axis1] = i;
//                         ChunkItr[Axis2] = j;
//
//                         // Find width
//                         int Width = 1;
//                         while (i + Width < InnerAxisSize1 && CompareMask(Mask[N + Width], Cell))
//                             ++Width;
//
//                         // Find height
//                         int Height = 1;
//                         bool bStop = false;
//                         while (j + Height < InnerAxisSize2 && !bStop)
//                         {
//                             for (int k = 0; k < Width; ++k)
//                             {
//                                 if (!CompareMask(Mask[N + k + Height * InnerAxisSize1], Cell))
//                                 {
//                                     bStop = true;
//                                     break;
//                                 }
//                             }
//                             if (!bStop) ++Height;
//                         }
//
//                         // Prepare deltas for quad
//                         DeltaAxis1 = FIntVector::ZeroValue;
//                         DeltaAxis2 = FIntVector::ZeroValue;
//                         DeltaAxis1[Axis1] = Width;
//                         DeltaAxis2[Axis2] = Height;
//
//                         // Create merged quad
//                         CreateQuad(
//                             Cell, AxisMask, Width, Height,
//                             ChunkItr,
//                             ChunkItr + DeltaAxis1,
//                             ChunkItr + DeltaAxis2,
//                             ChunkItr + DeltaAxis1 + DeltaAxis2
//                         );
//
//                         // Clear mask region
//                         for (int h = 0; h < Height; ++h)
//                         {
//                             for (int w = 0; w < Width; ++w)
//                             {
//                                 Mask[N + w + h * InnerAxisSize1] = FMask{EBlock::Air, 0};
//                             }
//                         }
//
//                         i += Width;
//                         N += Width;
//                     }
//                     else
//                     {
//                         ++i;
//                         ++N;
//                     }
//                 }
//             }
//         }
//     }
// }
//
// void AGreedyChunk::CreateQuad(
//     const FMask& Mask,
//     const FIntVector& AxisMask,
//     const int Width,
//     const int Height,
//     const FIntVector& V1,
//     const FIntVector& V2,
//     const FIntVector& V3,
//     const FIntVector& V4
// )
// {
//     const FVector Normal = FVector(AxisMask * Mask.Normal);
//     const FColor Color = FColor(0, 0, 0, GetTextureIndex(Mask.Block, Normal));
//
//     FChunkMeshData& MeshData = GetMeshDataForBlock(Mask.Block);
//     int& VertexCount = GetVertexCountForBlock(Mask.Block);
//
//     MeshData.Vertices.Append({
//         FVector(V1) * FChunkData::GetScaledBlockSize(this),
//         FVector(V2) * FChunkData::GetScaledBlockSize(this),
//         FVector(V3) * FChunkData::GetScaledBlockSize(this),
//         FVector(V4) * FChunkData::GetScaledBlockSize(this)
//     });
//
//     MeshData.Triangles.Append({
//         VertexCount,
//         VertexCount + 2 + Mask.Normal,
//         VertexCount + 2 - Mask.Normal,
//         VertexCount + 3,
//         VertexCount + 1 - Mask.Normal,
//         VertexCount + 1 + Mask.Normal
//     });
//
//     MeshData.Normals.Append({ Normal, Normal, Normal, Normal });
//     MeshData.Colors.Append({ Color, Color, Color, Color });
//
//     if (FMath::Abs(Normal.X) == 1)
//     {
//         MeshData.UV.Append({
//             FVector2D(Width, Height),
//             FVector2D(0, Height),
//             FVector2D(Width, 0),
//             FVector2D(0, 0),
//         });
//     }
//     else
//     {
//         MeshData.UV.Append({
//             FVector2D(Height, Width),
//             FVector2D(Height, 0),
//             FVector2D(0, Width),
//             FVector2D(0, 0),
//         });
//     }
//
//     VertexCount += 4;
// }
//
// bool AGreedyChunk::CompareMask(const FMask& M1, const FMask& M2) const
// {
//     return M1.Block == M2.Block && M1.Normal == M2.Normal;
// }