// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chunk.h"

#include <stdexcept>

#include "ProceduralMeshComponent.h"
#include "FastNoiseWrapper.h"
#include "VoxelGen/Enums.h"

AChunk::AChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);

	Noise = CreateDefaultSubobject<UFastNoiseWrapper>("Noise");
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();

	if (Noise)
	{
		Noise->SetupFastNoise(EFastNoise_NoiseType::Perlin, 1337, Frequency, EFastNoise_Interp::Quintic, EFastNoise_FractalType::FBM);
	}
	
	Blocks.SetNum(ChunkSize * ChunkSize * ChunkSize);

	GenerateBlocks();
	
	GenerateMesh();
	
	ApplyMesh();
}

void AChunk::GenerateBlocks()
{
	if (!Noise) return;
	
	const FVector Location = GetActorLocation();

	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			const float XPos = (x * BlockSize + Location.X) / BlockSize;
			const float YPos = (y * BlockSize + Location.Y) / BlockSize;

			// Get the height of the block at the current x and y position by getting the noise value at the x and y position and then scaling it to the chunk height
			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise2D(XPos, YPos) + 1) * ChunkSize / 2), 0, ChunkSize);

			for (int z = 0; z < Height; ++z)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
			}

			for (int z = Height; z < ChunkSize; ++z)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
			}
		}
	}
}

void AChunk::GenerateMesh()
{
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			for (int z = 0; z < ChunkSize; ++z)
			{
				if (Blocks[GetBlockIndex(x, y, z)] == EBlock::Air) continue;
				
				for (const EDirection Direction : { EDirection::Forward, EDirection::Right, EDirection::Backward, EDirection::Left, EDirection::Up, EDirection::Down })
				{
					FVector Position = FVector(x, y, z);
					if (CheckIsAir(GetPositionInDirection(Direction, Position)))
					{
						CreateFace(Direction, Position * BlockSize);
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
	if (Position.X >= 0 && Position.X < ChunkSize &&
		Position.Y >= 0 && Position.Y < ChunkSize &&
		Position.Z >= 0 && Position.Z < ChunkSize)
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
	if (X < 0 || X >= ChunkSize || Y < 0 || Y >= ChunkSize || Z < 0 || Z >= ChunkSize)
	{
		throw std::out_of_range("Block out of chunk range");
	}
	return X + Y * ChunkSize + Z * ChunkSize * ChunkSize;
}

