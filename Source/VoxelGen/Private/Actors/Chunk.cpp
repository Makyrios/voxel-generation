// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Chunk.h"

#include "ProceduralMeshComponent.h"
#include "FastNoiseWrapper.h"
#include "Actors/ChunkWorld.h"
#include "Structs/ChunkData.h"
#include "Objects/FChunkMeshLoaderAsync.h"
#include "Objects/TerrainGenerator.h"
#include "VoxelGen/Enums.h"

AChunk::AChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);
	
	TerrainGenerator = CreateDefaultSubobject<UTerrainGenerator>("TerrainGenerator");
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();
	
	Blocks.SetNum(FChunkData::ChunkSize * FChunkData::ChunkSize * FChunkData::ChunkHeight);
}

void AChunk::GenerateBlocks(const FVector& ChunkWorldPosition)
{
	if (!TerrainGenerator) return;

	TerrainGenerator->SetBiomeByName("Mountains");
	// TerrainGenerator->SetBiomeByName("Steppe");
	Blocks = TerrainGenerator->GenerateTerrain(ChunkWorldPosition);
}

void AChunk::GenerateMesh()
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

	bIsMeshInitialized = true;
}

void AChunk::ApplyMesh()
{
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		if (!Mesh) return;
		Mesh->CreateMeshSection(0, ChunkMeshData.Vertices, ChunkMeshData.Triangles, TArray<FVector>(), ChunkMeshData.UV, ChunkMeshData.Colors, TArray<FProcMeshTangent>(), true);

		if (Material)
		{
			Mesh->SetMaterial(0, Material);
		}

		bCanChangeBlocks = true;
	});
}

FIntVector AChunk::GetPositionInDirection(EDirection Direction, const FIntVector& Position) const
{
	FVector Pos(Position);
	
	switch (Direction)
	{
	case EDirection::Forward: return FIntVector(Pos + FVector::ForwardVector);
	case EDirection::Backward: return FIntVector(Pos + FVector::BackwardVector);
	case EDirection::Right: return FIntVector(Pos + FVector::RightVector);
	case EDirection::Left:	return FIntVector(Pos + FVector::LeftVector);
	case EDirection::Up:	return FIntVector(Pos + FVector::UpVector);
	case EDirection::Down: return FIntVector(Pos + FVector::DownVector);
	default: throw std::invalid_argument("Invalid direction");
	}
}

void AChunk::CreateFace(EDirection Direction, const FIntVector& Position)
{
	FVector WorldPosition(Position);
	WorldPosition *= FChunkData::BlockScaledSize;
	ChunkMeshData.Vertices.Append(GetFaceVerticies(Direction, WorldPosition));
	ChunkMeshData.UV.Append({ FVector2D(1, 0), FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1) });
	ChunkMeshData.Triangles.Append({ VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount });

	const int TextureIndex = GetTextureIndex(GetBlockAtPosition(Position), Direction);
	const FColor Color(0, 0, 0, TextureIndex);
	ChunkMeshData.Colors.Append({Color, Color, Color, Color});
	VertexCount += 4;
}

TArray<FVector> AChunk::GetFaceVerticies(EDirection Direction, const FVector& WorldPosition) const
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

int AChunk::GetTextureIndex(EBlock BlockType, EDirection Direction) const
{
	switch (BlockType)
	{
	case EBlock::Grass:
		if (Direction == EDirection::Up) return 0;
		if (Direction == EDirection::Down) return 2;
		return 1;
	case EBlock::Dirt: return 2;
	case EBlock::Stone: return 3;
	default: return 255;
	}
}

bool AChunk::CheckIsAir(const FIntVector& Position) const
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

void AChunk::RegenerateMeshAsync()
{
	(new FAutoDeleteAsyncTask<FChunkMeshLoaderAsync>(this))->StartBackgroundTask();
}

EBlock AChunk::GetBlockAtPosition(const FIntVector& Position) const
{
	if (IsWithinChunkBounds(Position))
	{
		return Blocks[FChunkData::GetBlockIndex(Position.X, Position.Y, Position.Z)];
	}
	
	FIntVector AdjBlockPosition;
	if (AChunk* AdjChunk = GetAdjacentChunk(Position, &AdjBlockPosition))
	{
		return AdjChunk->GetBlockAtPosition(AdjBlockPosition);
	}
	
	return EBlock::Air;
}

AChunk* AChunk::GetAdjacentChunk(const FIntVector& Position, FIntVector* const outAdjChunkBlockPosition) const
{
	if (!ParentWorld || !IsWithinVerticalBounds(Position)) return nullptr;

	FIntVector2 AdjChunkPos;
	FIntVector BlockPosition;
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

void AChunk::SetBlockAtPosition(const FIntVector& Position, EBlock BlockType)
{
	if (IsWithinChunkBounds(Position))
	{
		Blocks[FChunkData::GetBlockIndex(Position.X, Position.Y, Position.Z)] = BlockType;

		if (!bIsMeshInitialized) return;
		// Update the adjacent chunk only when destroying block to prevent updating the whole chunk mesh when spawning a block
		if (BlockType == EBlock::Air)
		{
			UpdateAdjacentChunk(Position);
		}
	}
	else
	{
		FIntVector AdjChunkBlockPosition;
		if (AChunk* AdjChunk = GetAdjacentChunk(Position, &AdjChunkBlockPosition))
		{
			AdjChunk->SpawnBlock(AdjChunkBlockPosition, BlockType);
		}
	}
}

void AChunk::UpdateAdjacentChunk(const FIntVector& LocalEdgeBlockPosition) const
{
	for (const auto& Offset : GetEdgeOffsets(LocalEdgeBlockPosition))
	{
		FIntVector AdjBlockPosition = LocalEdgeBlockPosition + Offset;
		if (AChunk* AdjacentChunk = GetAdjacentChunk(AdjBlockPosition))
		{
			AdjacentChunk->RegenerateMeshAsync();
		}
	}
}

TArray<FIntVector> AChunk::GetEdgeOffsets(const FIntVector& LocalEdgeBlockPosition) const
{
	TArray<FIntVector> Offsets;
	if (LocalEdgeBlockPosition.X == 0)
		Offsets.Add(FIntVector(-1, 0, 0));
	else if (LocalEdgeBlockPosition.X == FChunkData::ChunkSize - 1)
		Offsets.Add(FIntVector(1, 0, 0));

	if (LocalEdgeBlockPosition.Y == 0)
		Offsets.Add(FIntVector(0, -1, 0));
	else if (LocalEdgeBlockPosition.Y == FChunkData::ChunkSize - 1)
		Offsets.Add(FIntVector(0, 1, 0));

	return Offsets;
}

bool AChunk::AdjustForAdjacentChunk(const FIntVector& Position, FIntVector2& AdjChunkPosition, FIntVector& AdjBlockPosition) const
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

// int AChunk::GetBlockIndex(int X, int Y, int Z) const
// {
// 	if (X < 0 || X >= FChunkData::ChunkSize || Y < 0 || Y >= FChunkData::ChunkSize || Z < 0 || Z >= FChunkData::ChunkHeight)
// 	{
// 		throw std::out_of_range("Block out of chunk range");
// 	}
// 	return X + Y * FChunkData::ChunkSize + Z * FChunkData::ChunkSize * FChunkData::ChunkHeight;
// }

bool AChunk::IsWithinChunkBounds(const FIntVector& Position) const
{
	return Position.X >= 0 && Position.X < FChunkData::ChunkSize &&
		Position.Y >= 0 && Position.Y < FChunkData::ChunkSize &&
		Position.Z >= 0 && Position.Z < FChunkData::ChunkHeight;
}

bool AChunk::IsWithinVerticalBounds(const FIntVector& Position) const
{
	return Position.Z >= 0 && Position.Z < FChunkData::ChunkHeight;
}

void AChunk::SpawnBlock(const FIntVector& LocalChunkBlockPosition, EBlock BlockType)
{
	if (!bCanChangeBlocks) return;
	
	if (GetBlockAtPosition(LocalChunkBlockPosition) != EBlock::Air) return;

	bCanChangeBlocks = false;
	SetBlockAtPosition(LocalChunkBlockPosition, BlockType);

	if (IsWithinChunkBounds(LocalChunkBlockPosition))
	{
		RegenerateMeshAsync();
	}
}

void AChunk::DestroyBlock(const FIntVector& LocalChunkBlockPosition)
{
	if (!bCanChangeBlocks) return;

	if (!IsWithinChunkBounds(LocalChunkBlockPosition)) return;

	bCanChangeBlocks = false;
	SetBlockAtPosition(LocalChunkBlockPosition, EBlock::Air);
	
	RegenerateMeshAsync();
}