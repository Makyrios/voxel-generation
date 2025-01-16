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
        const float Size = FChunkData::ChunkSize;
        const int ChunkHeight = FChunkData::ChunkHeight; // Height of the chunk
        Mask.SetNum(Size * ChunkHeight);

        // Sweep along the current axis
        for (ChunkItr[Axis] = -1; ChunkItr[Axis] < Size;)
        {
            int N = 0;

            // Traverse along Axis2 (e.g., Y)
            for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < Size; ++ChunkItr[Axis2])
            {
                // Traverse along Axis1 (e.g., X)
                for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < Size; ++ChunkItr[Axis1])
                {
                    const auto CurrentBlock = GetBlockAtPosition(ChunkItr);
                    const auto CompareBlock = GetBlockAtPosition(ChunkItr + AxisMask);

                    // Determine block visibility (air blocks are not visible)
                    const bool CurrentBlockOpaque = CurrentBlock != EBlock::Air;
                    const bool CompareBlockOpaque = CompareBlock != EBlock::Air;

                    // Update the mask based on block visibility
                    if (CurrentBlockOpaque == CompareBlockOpaque)
                    {
                        Mask[N++] = FMask{EBlock::Air, 0}; // No visible face
                    }
                    else if (CurrentBlockOpaque)
                    {
                        Mask[N++] = FMask{CurrentBlock, 1}; // Visible face for current block
                    }
                    else
                    {
                        Mask[N++] = FMask{CompareBlock, -1}; // Visible face for adjacent block
                    }
                }
            }

            // Move to the next slice along the current axis
            ++ChunkItr[Axis];
            N = 0;

            // Iterate over the mask to generate quads for visible faces
            for (int j = 0; j < Size; ++j)
            {
                for (int i = 0; i < Size;)
                {
                    // If there is a visible face at this mask position
                    if (Mask[N].Normal != 0)
                    {
                        const auto CurrentMask = Mask[N];
                        ChunkItr[Axis1] = i;
                        ChunkItr[Axis2] = j;

                        // Determine the width of the quad
                        int Width;
                        for (Width = 1; i + Width < Size && CompareMask(Mask[N + Width], CurrentMask); ++Width) {}

                        // Determine the height of the quad
                        int Height;
                        bool Done = false;

                        for (Height = 1; j + Height < Size; ++Height)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                if (!CompareMask(Mask[N + k + Height * Size], CurrentMask))
                                {
                                    Done = true; // Stop if a non-matching block is found
                                    break;
                                }
                            }

                            if (Done) break;
                        }

                        // Set deltas for quad dimensions
                        DeltaAxis1[Axis1] = Width;
                        DeltaAxis2[Axis2] = Height;

                        // Create a quad for the current visible face
                        CreateQuad(
                            CurrentMask, AxisMask, Width, Height,
                            ChunkItr,
                            ChunkItr + DeltaAxis1,
                            ChunkItr + DeltaAxis2,
                            ChunkItr + DeltaAxis1 + DeltaAxis2
                        );

                        // Reset delta values
                        DeltaAxis1 = FIntVector::ZeroValue;
                        DeltaAxis2 = FIntVector::ZeroValue;

                        // Mark the area covered by the quad as processed
                        for (int l = 0; l < Height; ++l)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                Mask[N + k + l * Size] = FMask{EBlock::Air, 0};
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
    const FMask Mask,
    const FIntVector AxisMask,
    const int Width,
    const int Height,
    const FIntVector V1,
    const FIntVector V2,
    const FIntVector V3,
    const FIntVector V4
)
{
    const auto Normal = FVector(AxisMask * Mask.Normal);
    const auto Color = FColor(0, 0, 0, GetTextureIndex(Mask.Block, Normal));

    ChunkMeshData.Vertices.Append({
        FVector(V1) * FChunkData::BlockScaledSize,
        FVector(V2) * FChunkData::BlockScaledSize,
        FVector(V3) * FChunkData::BlockScaledSize,
        FVector(V4) * FChunkData::BlockScaledSize
    });

    ChunkMeshData.Triangles.Append({
        VertexCount,
        VertexCount + 2 + Mask.Normal,
        VertexCount + 2 - Mask.Normal,
        VertexCount + 3,
        VertexCount + 1 - Mask.Normal,
        VertexCount + 1 + Mask.Normal
    });

    ChunkMeshData.Normals.Append({ Normal, Normal, Normal, Normal });

    ChunkMeshData.Colors.Append({ Color, Color, Color, Color });

    if (Normal.X == 1 || Normal.X == -1)
    {
        ChunkMeshData.UV.Append({
            FVector2D(Width, Height),
            FVector2D(0, Height),
            FVector2D(Width, 0),
            FVector2D(0, 0),
        });
    }
    else
    {
        ChunkMeshData.UV.Append({
            FVector2D(Height, Width),
            FVector2D(Height, 0),
            FVector2D(0, Width),
            FVector2D(0, 0),
        });
    }

    VertexCount += 4;
}

bool AGreedyChunk::CompareMask(const FMask M1, const FMask M2) const
{
    return M1.Block == M2.Block && M1.Normal == M2.Normal;
}
