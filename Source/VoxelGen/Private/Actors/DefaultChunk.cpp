// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/DefaultChunk.h"

#include "Actors/ChunkWorld.h"
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

            	if (CurrentBlockProperties->RenderMode == EBlockRenderMode::Cube)
            	{
            		CreateCubePlanes(CurrentBlockPos, CurrentBlockType, CurrentBlockProperties);
				}
            	else if (CurrentBlockProperties->RenderMode == EBlockRenderMode::CrossPlanes)
            	{
            		CreateCrossPlanes(CurrentBlockPos, CurrentBlockType, CurrentBlockProperties);
            	}
				else if (CurrentBlockProperties->RenderMode == EBlockRenderMode::CustomMesh)
				{
				}
            }
        }
    }
}

void ADefaultChunk::CreateCrossPlanes(const FIntVector& CurrentBlockPos, EBlock Block, const FBlockSettings*& BlockSettings)
{
	if (!ParentWorld) return;
	
	// get mesh buffer & vertex counter
    FChunkMeshData& ChunkMeshData = GetMeshDataForBlock(Block);
    int& VertexCount = GetVertexCountForBlock(Block);

	// Pick a random texture variant for this block
	int32 Seed = ParentWorld->Seed
		   ^ (CurrentBlockPos.X * 73856093)
		   ^ (CurrentBlockPos.Y * 19349663)
		   ^ (CurrentBlockPos.Z * 83492791);
	FRandomStream Stream(Seed);
	
	int32 NumVariants = BlockSettings->NumTextureVariants;
	int32 VariantIndex = Stream.RandRange(0, NumVariants-1);

    // world space origin of this block
    const float ScaledBlockSize = FChunkData::GetScaledBlockSize(this);
	
    FVector Origin((CurrentBlockPos.X + 0.5f) * ScaledBlockSize, (CurrentBlockPos.Y + 0.5f) * ScaledBlockSize, CurrentBlockPos.Z * ScaledBlockSize);

    // size of the planes
    float HalfWidth = 0.5f * BlockSettings->RenderScale * ScaledBlockSize;
    float Height = BlockSettings->RenderHeight * ScaledBlockSize;

    // Define the four corners of a plane centered at Origin + (0,0,H/2)
    FVector A(-HalfWidth,  0, Height * 0.5f);
    FVector B( HalfWidth,  0, Height * 0.5f);
    FVector C( HalfWidth,  0,    0);
    FVector D(-HalfWidth,  0,    0);

	// Randomize rotation
	if (BlockSettings->RandomRotation)
	{
		float Yaw = Stream.FRandRange(0.f, 360.f);
		FQuat Rot(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		
		A = Rot.RotateVector(A);
		B = Rot.RotateVector(B);
		C = Rot.RotateVector(C);
		D = Rot.RotateVector(D);
	}

    // Two quads, rotated 90° around Z
    TArray<FVector> Verts;
    // Plane 1 (along X)
    Verts.Add(Origin + A);
    Verts.Add(Origin + B);
    Verts.Add(Origin + C);
    Verts.Add(Origin + D);

    // Plane 2 (along Y) — just swap X/Y
    Verts.Add(Origin + FVector( 0, -HalfWidth, Height * 0.5f));
    Verts.Add(Origin + FVector( 0,  HalfWidth, Height * 0.5f));
    Verts.Add(Origin + FVector( 0,  HalfWidth,    0));
    Verts.Add(Origin + FVector( 0, -HalfWidth,    0));

    // Append vertices
    ChunkMeshData.Vertices.Append(Verts);
	
	
    // UVs (same for both quads)
    for (int i = 0; i < 2; ++i)
    {
        ChunkMeshData.UV.Add(FVector2D(1, 0));
        ChunkMeshData.UV.Add(FVector2D(0, 0));
        ChunkMeshData.UV.Add(FVector2D(0, 1));
        ChunkMeshData.UV.Add(FVector2D(1, 1));
    }

    // Triangles (two per quad)
    for (int q = 0; q < 2; ++q)
    {
        int base = VertexCount + q * 4;
        ChunkMeshData.Triangles.Append({
            base+0, base+1, base+2,
            base+2, base+3, base+0
        });
    }

    FVector Normal(0, 0, 1);
    for (int i = 0; i < 8; ++i)
    {
        ChunkMeshData.Normals.Add(Normal);
        FColor col(0,0,0, VariantIndex);
        ChunkMeshData.Colors.Add(col);
    }

    VertexCount += 8;
}

void ADefaultChunk::CreateCubePlanes(const FIntVector& CurrentBlockPos, EBlock Block, const FBlockSettings*& BlockSettings)
{
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
			if (BlockSettings->MaterialType == EBlockMaterialType::Water)
			{
				EBlock NeighborTypeIfActuallyChecked = GetBlockAtPosition(NeighborPos);
				const FBlockSettings* NeighborPropsIfActuallyChecked = GetBlockData(NeighborTypeIfActuallyChecked);

				if (NeighborPropsIfActuallyChecked &&
					NeighborPropsIfActuallyChecked->MaterialType == EBlockMaterialType::Water &&
					Block == NeighborTypeIfActuallyChecked)
				{
					bActuallyDrawThisFace = false;
				}
			}

			if (bActuallyDrawThisFace)
			{
				CreateFace(Direction, CurrentBlockPos, Block, BlockSettings);
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