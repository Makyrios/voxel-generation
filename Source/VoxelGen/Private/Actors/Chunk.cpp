// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chunk.h"

#include "ProceduralMeshComponent.h"
#include "FastNoiseWrapper.h"
#include "Actors/ChunkWorld.h"
#include "Objects/ChunkData.h"
#include "VoxelGen/Enums.h"

AChunk::AChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;
	
	Noise = CreateDefaultSubobject<UFastNoiseWrapper>("Noise");
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();

	Noise->SetupFastNoise(EFastNoise_NoiseType::Perlin, 1337, Frequency, EFastNoise_Interp::Quintic, EFastNoise_FractalType::FBM);
	
	Blocks.SetNum(FChunkData::ChunkSize * FChunkData::ChunkSize * FChunkData::ChunkSize);

	GenerateBlocks();
	
	GenerateMesh();
	
	ApplyMesh();
}

void AChunk::GenerateBlocks()
{
	if (!Noise) return;
	
	const FVector Location = GetActorLocation();

	for (int x = 0; x < FChunkData::ChunkSize; ++x)
	{
		for (int y = 0; y < FChunkData::ChunkSize; ++y)
		{
			const float XPos = (x * FChunkData::BlockSize * FChunkData::BlockScale + Location.X) / (FChunkData::BlockSize * FChunkData::BlockScale);
			const float YPos = (y * FChunkData::BlockSize * FChunkData::BlockScale + Location.Y) / (FChunkData::BlockSize * FChunkData::BlockScale);

			// Get the height of the block at the current x and y position by getting the noise value at the x and y position and then scaling it to the chunk height
			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise2D(XPos, YPos) + 1) * FChunkData::ChunkSize * FChunkData::BlockScale / 2), 0, FChunkData::ChunkSize);

			for (int z = 0; z < Height; ++z)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
			}

			for (int z = Height; z < FChunkData::ChunkSize; ++z)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
			}
		}
	}
}

void AChunk::GenerateMesh()
{
	for (int x = 0; x < FChunkData::ChunkSize; ++x)
	{
		for (int y = 0; y < FChunkData::ChunkSize; ++y)
		{
			for (int z = 0; z < FChunkData::ChunkSize; ++z)
			{
				if (Blocks[GetBlockIndex(x, y, z)] == EBlock::Air) continue;
				
				for (const EDirection Direction : { EDirection::Forward, EDirection::Right, EDirection::Backward, EDirection::Left, EDirection::Up, EDirection::Down })
				{
					FVector Position = FVector(x, y, z);
					if (CheckIsAir(GetPositionInDirection(Direction, Position)))
					{
						CreateFace(Direction, Position * FChunkData::BlockSize * FChunkData::BlockScale);
					}
				}
			}
		}
	}
}

void AChunk::CreateFace(EDirection Direction, const FVector& Position)
{
	Verticies.Append(GetFaceVerticies(Direction, Position));
	UV.Append({ FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0) });
	Triangles.Append({ VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount });
	VertexCount += 4;
}

void AChunk::ApplyMesh() const
{
	Mesh->CreateMeshSection(0, Verticies, Triangles, TArray<FVector>(), UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	Mesh->AddCollisionConvexMesh(Verticies);
}

bool AChunk::CheckIsAir(const FVector& Position) const
{
	return GetBlockAtPosition(Position) == EBlock::Air;
}

void AChunk::RegenerateMesh()
{
	Verticies.Reset();
	UV.Reset();
	Triangles.Reset();
	VertexCount = 0;
	GenerateMesh();
	ApplyMesh();
}

EBlock AChunk::GetBlockAtPosition(const FVector& Position) const
{
	if (Position.X >= 0 && Position.X < FChunkData::ChunkSize &&
		Position.Y >= 0 && Position.Y < FChunkData::ChunkSize &&
		Position.Z >= 0 && Position.Z < FChunkData::ChunkSize)
	{
		return Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)];
	}
	else
	{
		if (!ParentWorld) return EBlock::Air;

		if (Position.Z < 0 || Position.Z >= FChunkData::ChunkSize) return EBlock::Air;
		
		FVector2D AdjChunkPos = ChunkPosition;
		FVector BlockPosition = Position;
		
		if (Position.X < 0)
		{
			AdjChunkPos.X -= 1;
			BlockPosition.X += FChunkData::ChunkSize;
		}
		else if (Position.X >= FChunkData::ChunkSize)
		{
			AdjChunkPos.X += 1;
			BlockPosition.X -= FChunkData::ChunkSize;
		}
		if (Position.Y < 0)
		{
			AdjChunkPos.Y -= 1;
			BlockPosition.Y += FChunkData::ChunkSize;
		}
		else if (Position.Y >= FChunkData::ChunkSize)
		{
			AdjChunkPos.Y += 1;
			BlockPosition.Y -= FChunkData::ChunkSize;
		}

		if (!ParentWorld->GetChunksData().Contains(AdjChunkPos)) return EBlock::Air;
		
		AChunk* adjChunk = ParentWorld->GetChunksData()[AdjChunkPos];
		if (!adjChunk) return EBlock::Air;
		
		return adjChunk->GetBlockAtPosition(BlockPosition);
	}
}

int AChunk::GetBlockIndex(int X, int Y, int Z) const
{
	if (X < 0 || X >= FChunkData::ChunkSize || Y < 0 || Y >= FChunkData::ChunkSize || Z < 0 || Z >= FChunkData::ChunkSize)
	{
		throw std::out_of_range("Block out of chunk range");
	}
	return X + Y * FChunkData::ChunkSize + Z * FChunkData::ChunkSize * FChunkData::ChunkSize;
}

TArray<FVector> AChunk::GetFaceVerticies(EDirection Direction, const FVector& Position) const
{
	TArray<FVector> FaceVerticies;

	for (int i = 0; i < 4; i++)
	{
		// Get the verticies for the face by getting the index of the verticies from the triangle data
		FaceVerticies.Add(BlockVerticies[BlockTriangles[static_cast<int>(Direction) * 4 + i]] * FChunkData::BlockScale + Position);
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

