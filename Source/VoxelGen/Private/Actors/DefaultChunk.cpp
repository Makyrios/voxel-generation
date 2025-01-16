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
	for (int x = 0; x < FChunkData::ChunkSize; ++x)
	{
		for (int y = 0; y < FChunkData::ChunkSize; ++y)
		{
			for (int z = 0; z < FChunkData::ChunkHeight; ++z)
			{
				if (GetBlockAtPosition(FIntVector(x, y, z)) == EBlock::Air) continue;
				
				for (const EDirection Direction : { EDirection::Forward, EDirection::Right, EDirection::Backward, EDirection::Left, EDirection::Up, EDirection::Down })
				{
					FIntVector Position(x, y, z);
					if (CheckIsAir(GetPositionInDirection(Direction, Position)))
					{
						CreateFace(Direction, Position);
					}
				}
			}
		}
	}
}

void ADefaultChunk::CreateFace(EDirection Direction, const FIntVector& Position)
{
	FVector WorldPosition(Position);
	WorldPosition *= FChunkData::BlockScaledSize;
	ChunkMeshData.Vertices.Append(GetFaceVerticies(Direction, WorldPosition));
	ChunkMeshData.UV.Append({ FVector2D(1, 0), FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1) });
	ChunkMeshData.Triangles.Append({ VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount });

	FVector Normal = GetNormal(Direction);
	const int TextureIndex = GetTextureIndex(GetBlockAtPosition(Position), Normal);
	const FColor Color(0, 0, 0, TextureIndex);
	ChunkMeshData.Colors.Append({Color, Color, Color, Color});
	VertexCount += 4;
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
		FaceVerticies.Add(BlockVerticies[BlockTriangles[static_cast<int>(Direction) * 4 + i]] * FChunkData::BlockScale + Pos);
	}

	return FaceVerticies;
}