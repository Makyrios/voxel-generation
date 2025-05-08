// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/DefaultChunk.h"

#include "VoxelGen/Enums.h"


ADefaultChunk::ADefaultChunk()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADefaultChunk::BeginPlay()
{
	Super::BeginPlay();
}

void ADefaultChunk::GenerateMesh()
{
	int32 ChunkSize = FChunkData::GetChunkSize(this);
    int32 ChunkHeight = FChunkData::GetChunkHeight(this);

    for (int x = 0; x < ChunkSize; ++x)
    {
        for (int y = 0; y < ChunkSize; ++y)
        {
            for (int z = 0; z < ChunkHeight; ++z)
            {
                FIntVector CurrentBlockPos(x, y, z);
                EBlock CurrentBlockType = GetBlockAtPosition(CurrentBlockPos);
                const FBlockSettings* CurrentBlockProperties = GetBlockData(CurrentBlockType);

                // Skip processing for Air blocks themselves or blocks with no defined properties
                if (CurrentBlockType == EBlock::Air || !CurrentBlockProperties)
                {
                    continue;
                }

                // Iterate through 6 directions
                for (int i = 0; i < 6; ++i)
                {
                    EDirection Direction = static_cast<EDirection>(i);
                    FIntVector NeighborPos = GetPositionInDirection(Direction, CurrentBlockPos);

                    // Use the FBlockSettings of the neighbor to determine if it's "see-through"
                    if (ShouldRenderFace(NeighborPos))
                    {
                        bool bActuallyDrawThisFace = true;
                    	
                        // Apply culling rules
                        // Cull internal faces of identical transparent blocks (water)
                        if (CurrentBlockProperties->MaterialType == EBlockMaterialType::Transparent)
                        {
                            EBlock NeighborTypeIfActuallyChecked = GetBlockAtPosition(NeighborPos);
                            const FBlockSettings* NeighborPropsIfActuallyChecked = GetBlockData(NeighborTypeIfActuallyChecked);

                            if (NeighborPropsIfActuallyChecked &&
                                NeighborPropsIfActuallyChecked->MaterialType == EBlockMaterialType::Transparent &&
                                CurrentBlockType == NeighborTypeIfActuallyChecked)
                            {
                                bActuallyDrawThisFace = false;
                            }
                        }

                    	// Leaves?

                        if (bActuallyDrawThisFace)
                        {
                            CreateFace(Direction, CurrentBlockPos, CurrentBlockType, CurrentBlockProperties);
                        }
                    }
                }
            }
        }
    }
}

void ADefaultChunk::CreateFace(EDirection Direction, const FIntVector& Position, EBlock BlockType, const FBlockSettings* BlockProperties)
{
    if (!BlockProperties) return;

    FChunkMeshData& TargetMeshData = GetMeshDataForBlock(BlockType);
    int& TargetVertexCount = GetVertexCountForBlock(BlockType);

    FVector WorldPositionOffset(Position);
    WorldPositionOffset *= FChunkData::GetScaledBlockSize(this);

    TargetMeshData.Vertices.Append(GetFaceVerticies(Direction, WorldPositionOffset));

    // Standard UVs for a quad
    TargetMeshData.UV.Add(FVector2D(1, 0));
    TargetMeshData.UV.Add(FVector2D(0, 0));
    TargetMeshData.UV.Add(FVector2D(0, 1));
    TargetMeshData.UV.Add(FVector2D(1, 1));

	// Triangles
    TargetMeshData.Triangles.Add(TargetVertexCount + 3);
    TargetMeshData.Triangles.Add(TargetVertexCount + 2);
    TargetMeshData.Triangles.Add(TargetVertexCount + 0);

    TargetMeshData.Triangles.Add(TargetVertexCount + 2);
    TargetMeshData.Triangles.Add(TargetVertexCount + 1);
    TargetMeshData.Triangles.Add(TargetVertexCount + 0);


    FVector Normal = GetNormal(Direction);
    // Get texture index from FBlockSettings based on face normal
    int TextureIndexValue = 255; // Default/error texture
    if (Normal == FVector::UpVector) TextureIndexValue = BlockProperties->TextureData.TopFaceTexture;
    else if (Normal == FVector::DownVector) TextureIndexValue = BlockProperties->TextureData.BottomFaceTexture;
    else TextureIndexValue = BlockProperties->TextureData.SideFaceTexture;

    const FColor Color(0, 0, 0, TextureIndexValue);
    TargetMeshData.Colors.Add(Color);
    TargetMeshData.Colors.Add(Color);
    TargetMeshData.Colors.Add(Color);
    TargetMeshData.Colors.Add(Color);

    TargetMeshData.Normals.Add(Normal);
    TargetMeshData.Normals.Add(Normal);
    TargetMeshData.Normals.Add(Normal);
    TargetMeshData.Normals.Add(Normal);

    TargetVertexCount += 4;
}

FVector ADefaultChunk::GetNormal(EDirection Direction)
{
	switch (Direction)
	{
	case EDirection::Forward: return FVector::ForwardVector;
	case EDirection::Right: return FVector::RightVector;
	case EDirection::Backward: return FVector::BackwardVector;
	case EDirection::Left: return FVector::LeftVector;
	case EDirection::Up: return FVector::UpVector;
	case EDirection::Down: return FVector::DownVector;
	default: throw std::invalid_argument("Invalid direction");
	}
}

TArray<FVector> ADefaultChunk::GetFaceVerticies(EDirection Direction, const FVector& WorldPosition) const
{
	TArray<FVector> FaceVerticies;

	for (int i = 0; i < 4; i++)
	{
		// Get the verticies for the face by getting the index of the verticies from the triangle data
		FVector Pos(WorldPosition.X, WorldPosition.Y, WorldPosition.Z);
		FaceVerticies.Add(BlockVerticies[BlockTriangles[static_cast<int>(Direction) * 4 + i]] * FChunkData::GetBlockScale(this) + Pos);
	}

	return FaceVerticies;
}