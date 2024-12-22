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
	SetRootComponent(Mesh);
	
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
				SetBlockAtPosition(FVector(x, y, z), EBlock::Stone);
			}

			for (int z = Height; z < FChunkData::ChunkSize; ++z)
			{
				SetBlockAtPosition(FVector(x, y, z), EBlock::Air);
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
				if (GetBlockAtPosition(FVector(x, y, z)) == EBlock::Air) continue;
				
				for (const EDirection Direction : { EDirection::Forward, EDirection::Right, EDirection::Backward, EDirection::Left, EDirection::Up, EDirection::Down })
				{
					FVector Position(x, y, z);
					if (CheckIsAir(GetPositionInDirection(Direction, Position)))
					{
						CreateFace(Direction, Position * FChunkData::BlockSize * FChunkData::BlockScale);
					}
				}
			}
		}
	}
}

FVector AChunk::GetPositionInDirection(EDirection Direction, const FVector& Position) const
{
	switch (Direction)
	{
	case EDirection::Forward: return Position + FVector::ForwardVector;
	case EDirection::Backward: return Position + FVector::BackwardVector;
	case EDirection::Right: return Position + FVector::RightVector;
	case EDirection::Left:	return Position + FVector::LeftVector;
	case EDirection::Up:	return Position + FVector::UpVector;
	case EDirection::Down: return Position + FVector::DownVector;
	default: throw std::invalid_argument("Invalid direction");
	}
}

void AChunk::CreateFace(EDirection Direction, const FVector& Position)
{
	ChunkMeshData.Vertices.Append(GetFaceVerticies(Direction, Position));
	ChunkMeshData.UV.Append({ FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0) });
	ChunkMeshData.Triangles.Append({ VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount });
	VertexCount += 4;
}

TArray<FVector> AChunk::GetFaceVerticies(EDirection Direction, const FVector& Position) const
{
	TArray<FVector> FaceVerticies;

	for (int i = 0; i < 4; i++)
	{
		// Get the verticies for the face by getting the index of the verticies from the triangle data
		FVector Pos(Position.X, Position.Y, Position.Z);
		FaceVerticies.Add(BlockVerticies[BlockTriangles[static_cast<int>(Direction) * 4 + i]] * FChunkData::BlockScale + Pos);
	}

	return FaceVerticies;
}

void AChunk::ApplyMesh() const
{
	Mesh->CreateMeshSection(0, ChunkMeshData.Vertices, ChunkMeshData.Triangles, TArray<FVector>(), ChunkMeshData.UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
}

bool AChunk::CheckIsAir(const FVector& Position) const
{
	return GetBlockAtPosition(Position) == EBlock::Air;
}

void AChunk::RegenerateMesh()
{
	ChunkMeshData.Clear();
	VertexCount = 0;
	GenerateMesh();
	ApplyMesh();
}

EBlock AChunk::GetBlockAtPosition(const FVector& Position) const
{
	if (IsWithinChunkBounds(Position))
	{
		return Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)];
	}
	else
	{
		FVector AdjBlockPosition;
		if (AChunk* AdjChunk = GetAdjacentChunk(Position, &AdjBlockPosition))
		{
			return AdjChunk->GetBlockAtPosition(AdjBlockPosition);
		}
	}
	return EBlock::Air;
}

AChunk* AChunk::GetAdjacentChunk(const FVector& Position, FVector* const outAdjChunkBlockPosition) const
{
	if (!ParentWorld || !IsWithinVerticalBounds(Position)) return nullptr;

	FVector2D AdjChunkPos;
	FVector BlockPosition;
	if (!AdjustForAdjacentChunk(Position, AdjChunkPos, BlockPosition)) return nullptr;

	if (outAdjChunkBlockPosition)
	{
		*outAdjChunkBlockPosition = BlockPosition;
	}
	
	if (ParentWorld->GetChunksData().Contains(AdjChunkPos))
	{
		return ParentWorld->GetChunksData().FindRef(AdjChunkPos);
	}
	return nullptr;
}

void AChunk::SetBlockAtPosition(const FVector& Position, EBlock BlockType)
{
	if (IsWithinChunkBounds(Position))
	{
		Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)] = BlockType;

		// Update the adjacent chunk only when destroying block
		// to prevent updating the whole chunk mesh when spawning a block
		if (BlockType == EBlock::Air)
		{
			UpdateAdjacentChunk(Position);
		}
	}
	else
	{
		FVector AdjChunkBlockPosition;
		if (AChunk* AdjChunk = GetAdjacentChunk(Position, &AdjChunkBlockPosition))
		{
			AdjChunk->SpawnBlock(AdjChunkBlockPosition, BlockType);
		}
	}
}

void AChunk::UpdateAdjacentChunk(const FVector& LocalEdgeBlockPosition) const
{
	for (const auto& Offset : GetEdgeOffsets(LocalEdgeBlockPosition))
	{
		FVector AdjBlockPosition = LocalEdgeBlockPosition + Offset;
		if (AChunk* AdjacentChunk = GetAdjacentChunk(AdjBlockPosition))
		{
			AdjacentChunk->RegenerateMesh();
		}
	}
}

TArray<FVector> AChunk::GetEdgeOffsets(const FVector& LocalEdgeBlockPosition) const
{
	TArray<FVector> Offsets;
	if (LocalEdgeBlockPosition.X == 0)
		Offsets.Add(FVector(-1, 0, 0));
	else if (LocalEdgeBlockPosition.X == FChunkData::ChunkSize - 1)
		Offsets.Add(FVector(1, 0, 0));

	if (LocalEdgeBlockPosition.Y == 0)
		Offsets.Add(FVector(0, -1, 0));
	else if (LocalEdgeBlockPosition.Y == FChunkData::ChunkSize - 1)
		Offsets.Add(FVector(0, 1, 0));

	return Offsets;
}

bool AChunk::AdjustForAdjacentChunk(const FVector& Position, FVector2D& AdjChunkPosition, FVector& AdjBlockPosition) const
{
	if (!IsWithinVerticalBounds(Position)) return false;

	AdjChunkPosition = ChunkPosition;
	AdjBlockPosition = Position;

	if (Position.X < 0)
	{
		AdjChunkPosition.X -= 1;
		AdjBlockPosition.X += FChunkData::ChunkSize;
	}
	else if (Position.X >= FChunkData::ChunkSize)
	{
		AdjChunkPosition.X += 1;
		AdjBlockPosition.X -= FChunkData::ChunkSize;
	}
	if (Position.Y < 0)
	{
		AdjChunkPosition.Y -= 1;
		AdjBlockPosition.Y += FChunkData::ChunkSize;
	}
	else if (Position.Y >= FChunkData::ChunkSize)
	{
		AdjChunkPosition.Y += 1;
		AdjBlockPosition.Y -= FChunkData::ChunkSize;
	}

	return true;
}

int AChunk::GetBlockIndex(int X, int Y, int Z) const
{
	if (X < 0 || X >= FChunkData::ChunkSize || Y < 0 || Y >= FChunkData::ChunkSize || Z < 0 || Z >= FChunkData::ChunkSize)
	{
		throw std::out_of_range("Block out of chunk range");
	}
	return X + Y * FChunkData::ChunkSize + Z * FChunkData::ChunkSize * FChunkData::ChunkSize;
}

bool AChunk::IsWithinChunkBounds(const FVector& Position) const
{
	return Position.X >= 0 && Position.X < FChunkData::ChunkSize &&
		Position.Y >= 0 && Position.Y < FChunkData::ChunkSize &&
		Position.Z >= 0 && Position.Z < FChunkData::ChunkSize;
}

bool AChunk::IsWithinVerticalBounds(const FVector& Position) const
{
	return Position.Z >= 0 && Position.Z < FChunkData::ChunkSize;
}

void AChunk::SpawnBlock(const FVector& LocalChunkBlockPosition, EBlock BlockType)
{
	if (GetBlockAtPosition(LocalChunkBlockPosition) != EBlock::Air) return;
	
	SetBlockAtPosition(LocalChunkBlockPosition, BlockType);

	if (IsWithinChunkBounds(LocalChunkBlockPosition))
	{
		RegenerateMesh();
	}
}

void AChunk::DestroyBlock(const FVector& LocalChunkBlockPosition)
{
	if (!IsWithinChunkBounds(LocalChunkBlockPosition)) return;

	SetBlockAtPosition(LocalChunkBlockPosition, EBlock::Air);

	RegenerateMesh();
}