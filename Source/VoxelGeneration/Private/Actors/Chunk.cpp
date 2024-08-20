// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chunk.h"

#include <stdexcept>

#include "ProceduralMeshComponent.h"
#include "FastNoiseWrapper.h"
#include "VoxelGeneration/Enums.h"

AChunk::AChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);

	Noise = CreateDefaultSubobject<UFastNoiseWrapper>("Noise");
	Noise->SetupFastNoise(EFastNoise_NoiseType::Perlin);
	Noise->SetFrequency(Frequency);
	Noise->SetFractalType(EFastNoise_FractalType::FBM);

	Blocks.SetNum(ChunkWidth * ChunkWidth * ChunkHeight);
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();

	GenerateBlocks();
	
	GenerateMesh();
	
	ApplyMesh();
}

void AChunk::GenerateBlocks()
{
	const FVector Location = GetActorLocation();

	for (int x = 0; x < ChunkWidth; ++x)
	{
		for (int y = 0; y < ChunkWidth; ++y)
		{
			const float XPos = (x * 100 + Location.X) / 100;
			const float YPos = (y * 100 + Location.Y) / 100;

			// Get the height of the block at the current x and y position by getting the noise value at the x and y position and then scaling it to the chunk height
			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise2D(XPos, YPos) + 1) * ChunkHeight / 2), 0, ChunkHeight);

			for (int z = 0; z < Height; ++z)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
			}

			for (int z = Height; z < ChunkHeight; ++z)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
			}
		}
	}
}

void AChunk::GenerateMesh()
{
	for (int x = 0; x < ChunkWidth; ++x)
	{
		for (int y = 0; y < ChunkWidth; ++y)
		{
			for (int z = 0; z < ChunkHeight; ++z)
			{
				if (Blocks[GetBlockIndex(x, y, z)] != EBlock::Air)
				{
					for (EDirection Direction : { EDirection::Forward, EDirection::Right, EDirection::Backward, EDirection::Left, EDirection::Up, EDirection::Down })
					{
						if (CheckIsAir(GetPositionInDirection(Direction, FVector(x, y, z))))
						{
							CreateFace(Direction, FVector(x, y, z) * 100);
						}
					}
				}
			}
		}
	}
}

void AChunk::ApplyMesh() const
{
	Mesh->CreateMeshSection(0, Verticies, Triangles, TArray<FVector>(), UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}

bool AChunk::CheckIsAir(const FVector& Position) const
{
	if (Position.X >= 0 && Position.X < ChunkWidth &&
		Position.Y >= 0 && Position.Y < ChunkWidth &&
		Position.Z >= 0 && Position.Z < ChunkHeight)
	{
		return Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)] == EBlock::Air;
	}
	return true;
}

void AChunk::CreateFace(EDirection Direction, const FVector& Position)
{
	Verticies.Append(GetFaceVerticies(Direction, Position));
	UV.Append({ FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0) });
	Triangles.Append({ VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount });
	VertexCount += 4;
}

TArray<FVector> AChunk::GetFaceVerticies(EDirection Direction, const FVector& Position) const
{
	TArray<FVector> FaceVerticies;

	for (int i = 0; i < 4; i++)
	{
		// Get the verticies for the face by getting the index of the verticies from the triangle data
		FaceVerticies.Add(BlockVerticies[BlockTriangles[static_cast<int>(Direction) * 4 + i]] * Scale + Position);
	}

	return FaceVerticies;
}

FVector AChunk::GetPositionInDirection(EDirection Direction, const FVector& Position) const
{
	switch (Direction)
	{
	case EDirection::Forward: return Position + FVector::ForwardVector;
	case EDirection::Right: return Position + FVector::RightVector;
	case EDirection::Backward: return Position + FVector::BackwardVector;
	case EDirection::Left: return Position + FVector::LeftVector;
	case EDirection::Up: return Position + FVector::UpVector;
	case EDirection::Down: return Position + FVector::DownVector;
	default: throw std::invalid_argument("Invalid direction");	
	}
}

int AChunk::GetBlockIndex(int X, int Y, int Z) const
{
	return X + Y * ChunkWidth + Z * ChunkWidth * ChunkHeight;
}

