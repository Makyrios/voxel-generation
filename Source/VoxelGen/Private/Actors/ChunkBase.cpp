// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChunkBase.h"

#include "ProceduralMeshComponent.h"
#include "Actors/ChunkWorld.h"
#include "Structs/ChunkData.h"
#include "Objects/FChunkMeshLoaderAsync.h"
#include "Objects/TerrainGenerator.h"
#include "VoxelGen/Enums.h"

AChunkBase::AChunkBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);
	
	TerrainGenerator = CreateDefaultSubobject<UTerrainGenerator>("TerrainGenerator");
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();
	
	Blocks.SetNum(FChunkData::ChunkSize * FChunkData::ChunkSize * FChunkData::ChunkHeight);
}

void AChunkBase::ApplyMesh()
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
		bIsProcessingMesh = false;
		bIsMeshInitialized = true;
	});
	
}

FIntVector AChunkBase::GetPositionInDirection(EDirection Direction, const FIntVector& Position) const
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

int AChunkBase::GetTextureIndex(EBlock BlockType, const FVector& Normal) const
{
	switch (BlockType)
	{
	case EBlock::Grass:
		if (Normal == FVector::UpVector) return 0;
		if (Normal == FVector::DownVector) return 2;
		return 1;
	case EBlock::Dirt: return 2;
	case EBlock::Stone: return 3;
	default: return 255;
	}
}

bool AChunkBase::CheckIsAir(const FIntVector& Position) const
{
	return GetBlockAtPosition(Position) == EBlock::Air;
}

bool AChunkBase::CheckIsAir(int X, int Y, int Z) const
{
	return GetBlockAtPosition(FIntVector(X, Y, Z)) == EBlock::Air;
}

void AChunkBase::RegenerateMesh()
{
	ChunkMeshData.Clear();
	VertexCount = 0;
	GenerateMesh();
	ApplyMesh();
}

void AChunkBase::RegenerateMeshAsync()
{
	(new FAutoDeleteAsyncTask<FChunkMeshLoaderAsync>(this))->StartBackgroundTask();
}

void AChunkBase::ClearMesh()
{
	if (!Mesh) return;
	
	Mesh->ClearAllMeshSections();
	bIsMeshInitialized = false;
}

void AChunkBase::GenerateBlocks(const FVector& ChunkWorldPosition)
{
	if (!TerrainGenerator) return;
	
	// 	TerrainGenerator->SetBiomeByName("Steppe");
	TerrainGenerator->SetBiomeByName("Mountains");
	Blocks = TerrainGenerator->GenerateTerrain(ChunkWorldPosition);
}

EBlock AChunkBase::GetBlockAtPosition(const FIntVector& Position) const
{
	if (IsWithinChunkBounds(Position))
	{
		return Blocks[FChunkData::GetBlockIndex(Position.X, Position.Y, Position.Z)];
	}
	
	FIntVector AdjBlockPosition;
	if (AChunkBase* AdjChunk = GetAdjacentChunk(Position, &AdjBlockPosition))
	{
		return AdjChunk->GetBlockAtPosition(AdjBlockPosition);
	}
	
	return EBlock::Air;
}

EBlock AChunkBase::GetBlockAtPosition(int X, int Y, int Z) const
{
	if (IsWithinChunkBounds(FIntVector(X, Y, Z)))
	{
		return Blocks[FChunkData::GetBlockIndex(X, Y, Z)];
	}
	
	FIntVector AdjBlockPosition;
	if (AChunkBase* AdjChunk = GetAdjacentChunk(FIntVector(X, Y, Z), &AdjBlockPosition))
	{
		return AdjChunk->GetBlockAtPosition(AdjBlockPosition);
	}
	
	return EBlock::Air;
}

AChunkBase* AChunkBase::GetAdjacentChunk(const FIntVector& Position, FIntVector* const outAdjChunkBlockPosition) const
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

void AChunkBase::SetBlockAtPosition(const FIntVector& Position, EBlock BlockType)
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
		if (AChunkBase* AdjChunk = GetAdjacentChunk(Position, &AdjChunkBlockPosition))
		{
			AdjChunk->SpawnBlock(AdjChunkBlockPosition, BlockType);
		}
	}
}

void AChunkBase::UpdateAdjacentChunk(const FIntVector& LocalEdgeBlockPosition) const
{
	for (const auto& Offset : GetEdgeOffsets(LocalEdgeBlockPosition))
	{
		FIntVector AdjBlockPosition = LocalEdgeBlockPosition + Offset;
		if (AChunkBase* AdjacentChunk = GetAdjacentChunk(AdjBlockPosition))
		{
			AdjacentChunk->RegenerateMeshAsync();
		}
	}
}

TArray<FIntVector> AChunkBase::GetEdgeOffsets(const FIntVector& LocalEdgeBlockPosition) const
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

bool AChunkBase::AdjustForAdjacentChunk(const FIntVector& Position, FIntVector2& AdjChunkPosition, FIntVector& AdjBlockPosition) const
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

bool AChunkBase::IsWithinChunkBounds(const FIntVector& Position) const
{
	return Position.X >= 0 && Position.X < FChunkData::ChunkSize &&
		Position.Y >= 0 && Position.Y < FChunkData::ChunkSize &&
		Position.Z >= 0 && Position.Z < FChunkData::ChunkHeight;
}

bool AChunkBase::IsWithinVerticalBounds(const FIntVector& Position) const
{
	return Position.Z >= 0 && Position.Z < FChunkData::ChunkHeight;
}

void AChunkBase::SpawnBlock(const FIntVector& LocalChunkBlockPosition, EBlock BlockType)
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

void AChunkBase::DestroyBlock(const FIntVector& LocalChunkBlockPosition)
{
	if (!bCanChangeBlocks) return;

	if (!IsWithinChunkBounds(LocalChunkBlockPosition)) return;

	bCanChangeBlocks = false;
	SetBlockAtPosition(LocalChunkBlockPosition, EBlock::Air);
	
	RegenerateMeshAsync();
}