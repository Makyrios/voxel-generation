// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ChunkBase.h"

#include "ProceduralMeshComponent.h"
#include "Actors/ChunkWorld.h"
#include "Structs/ChunkData.h"
#include "Objects/ChunkMeshLoaderAsync.h"
#include "Structs/BlockSettings.h"
#include "Structs/ChunkColumn.h"
#include "VoxelGen/Enums.h"


AChunkBase::AChunkBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();

	const float BlockSize = FChunkData::GetBlockSize(this);

	BlockVerticies = {
		FVector(BlockSize,BlockSize,BlockSize),
			FVector(BlockSize,0,BlockSize),
			FVector(BlockSize,0,0),
			FVector(BlockSize,BlockSize,0),
			FVector(0,0,BlockSize),
			FVector(0,BlockSize,BlockSize),
			FVector(0,BlockSize,0),
			FVector(0,0,0)
	};
	
	ChunkColumns.SetNum(FChunkData::GetChunkSize(this) * FChunkData::GetChunkSize(this));
}

void AChunkBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Cancel any ongoing mesh processing
	bIsProcessingMesh = false;
}

void AChunkBase::ApplyMesh()
{
	if (!IsValid(Mesh))
	{
		return;
	}

	Mesh->ClearAllMeshSections();

	// Create section for Opaque blocks (Material Slot 0)
	if (OpaqueChunkMeshData.Vertices.Num() > 0 && OpaqueMaterial)
	{
		Mesh->CreateMeshSection(0, OpaqueChunkMeshData.Vertices, OpaqueChunkMeshData.Triangles,
								TArray<FVector>(), OpaqueChunkMeshData.UV, OpaqueChunkMeshData.Colors,
								TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(0, OpaqueMaterial);
	}

	// Create section for Translucent blocks (Material Slot 1)
	if (WaterChunkMeshData.Vertices.Num() > 0 && WaterMaterial)
	{
		Mesh->CreateMeshSection(1, WaterChunkMeshData.Vertices, WaterChunkMeshData.Triangles,
								TArray<FVector>(), WaterChunkMeshData.UV, WaterChunkMeshData.Colors,
								TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(1, WaterMaterial);
	}

	// Create section for Masked blocks (Material Slot 2)
	if (MaskedChunkMeshData.Vertices.Num() > 0 && MaskedMaterial)
	{
		Mesh->CreateMeshSection(2, MaskedChunkMeshData.Vertices, MaskedChunkMeshData.Triangles,
								TArray<FVector>(), MaskedChunkMeshData.UV, MaskedChunkMeshData.Colors,
								TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(2, MaskedMaterial);
	}

	bCanChangeBlocks = true;
	bIsProcessingMesh = false;
	bIsMeshInitialized = true;

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
	const FBlockSettings* Data = GetBlockData(BlockType);
	if (!Data) return 255; // Default/Error texture

	// Simplified example, assumes your texture array matches this order conceptually
	if (Normal == FVector::UpVector) return Data->TextureData.TopFaceTexture;
	if (Normal == FVector::DownVector) return Data->TextureData.BottomFaceTexture;
	return Data->TextureData.SideFaceTexture;
}

const FBlockSettings* AChunkBase::GetBlockData(EBlock BlockType) const
{
	if (!BlockDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockDataTable is not set in ChunkBase!"));
		return nullptr;
	}
	
	const FString EnumString = UEnum::GetValueAsString(BlockType);
	const FName RowName = FName(EnumString.RightChop(EnumString.Find(TEXT("::")) + 2));
	
	FBlockSettings* FoundRow = BlockDataTable->FindRow<FBlockSettings>(RowName, TEXT("Looking up BlockData"));
	if (!FoundRow)
	{
		// Fallback for Air or undefined blocks
		static const FBlockSettings AirDataDefault;
		if (BlockType == EBlock::Air) return &AirDataDefault;

		UE_LOG(LogTemp, Warning, TEXT("BlockData not found for type: %s"), *RowName.ToString());
		return nullptr;
	}
	return FoundRow;
}

bool AChunkBase::ShouldRenderFace(const FIntVector& Position) const
{
	EBlock Block = GetBlockAtPosition(Position);
	if (Block == EBlock::Air) return true;

	const FBlockSettings* Data = GetBlockData(Block);
	// A block is considered "air" for culling purposes if it's not solid OR if it's transparent.
	return (Data && (!Data->bIsSolid || Data->bIsTransparent));
}

bool AChunkBase::ShouldRenderFace(int X, int Y, int Z) const
{
	return GetBlockAtPosition(FIntVector(X, Y, Z)) == EBlock::Air;
}

void AChunkBase::RegenerateMesh()
{
	// Clear all mesh data stores
	OpaqueChunkMeshData.Clear();
	WaterChunkMeshData.Clear();
	MaskedChunkMeshData.Clear();

	OpaqueVertexCount = 0;
	WaterVertexCount = 0;
	MaskedVertexCount = 0;

	GenerateMesh();
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		ApplyMesh();
		ParentWorld->NotifyMeshTaskCompleted();
	});
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

void AChunkBase::SetColumns(const TArray<FChunkColumn>& NewColumns)
{
	ChunkColumns = NewColumns;
}

EBlock AChunkBase::GetBlockAtPosition(const FIntVector& Position) const
{
	if (!GetWorld()) return EBlock::Air;
	
 	const int32 ChunkSize = FChunkData::GetChunkSize(GetWorld());
    const int32 ChunkHeight = FChunkData::GetChunkHeight(GetWorld());
    
    if (Position.X >= 0 && Position.X < ChunkSize && 
        Position.Y >= 0 && Position.Y < ChunkSize &&
        Position.Z >= 0 && Position.Z < ChunkHeight)
    {
        const int32 ColumnIndex = Position.X + (Position.Y * ChunkSize);
        
        if (ColumnIndex >= 0 && ColumnIndex < ChunkColumns.Num())
        {
            return ChunkColumns[ColumnIndex].Blocks[Position.Z];
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Invalid column index: %d"), ColumnIndex);
        return EBlock::Air;
    }
    
    // Handle out-of-bounds positions with adjacent chunks
    if (Position.Z >= 0 && Position.Z < ChunkHeight)
    {
        FIntVector2 AdjChunkPos = ChunkPosition;
        FIntVector LocalPos = Position;
        
        // Calculate adjacent chunk coordinates without allocating a new object
        if (Position.X < 0)
        {
            AdjChunkPos.X -= 1;
            LocalPos.X += ChunkSize;
        }
        else if (Position.X >= ChunkSize)
        {
            AdjChunkPos.X += 1;
            LocalPos.X -= ChunkSize;
        }
        
        if (Position.Y < 0)
        {
            AdjChunkPos.Y -= 1;
            LocalPos.Y += ChunkSize;
        }
        else if (Position.Y >= ChunkSize)
        {
            AdjChunkPos.Y += 1;
            LocalPos.Y -= ChunkSize;
        }
        
        // Lookup in adjacent chunk
    	if (!ParentWorld) return EBlock::Air;
        if (AChunkBase* AdjChunk = ParentWorld->GetChunksData().FindRef(AdjChunkPos))
        {
            const int32 AdjColumnIndex = LocalPos.X + (LocalPos.Y * ChunkSize);
            if (AdjColumnIndex >= 0 && AdjColumnIndex < AdjChunk->ChunkColumns.Num())
            {
                return AdjChunk->ChunkColumns[AdjColumnIndex].Blocks[LocalPos.Z];
            }
        }
    }
    return EBlock::Air;
}

EBlock AChunkBase::GetBlockAtPosition(int X, int Y, int Z) const
{
	return GetBlockAtPosition(FIntVector(X, Y, Z));
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
	
	if (AChunkBase* const* FoundChunkPtr = ParentWorld->GetChunksData().Find(AdjChunkPos))
	{
		return *FoundChunkPtr;
	}
	return nullptr;
}

void AChunkBase::SetBlockAtPosition(const FIntVector& Position, EBlock BlockType)
{
	if (IsWithinChunkBounds(Position))
	{
		ChunkColumns[FChunkData::GetColumnIndex(this, Position.X, Position.Y)].Blocks[Position.Z] = BlockType;

		if (!bIsMeshInitialized) return;
		// Update the adjacent chunk only when destroying block to prevent updating the whole chunk mesh when spawning a block
		if (BlockType == EBlock::Air || BlockType == EBlock::Water)
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
	else if (LocalEdgeBlockPosition.X == FChunkData::GetChunkSize(this) - 1)
		Offsets.Add(FIntVector(1, 0, 0));

	if (LocalEdgeBlockPosition.Y == 0)
		Offsets.Add(FIntVector(0, -1, 0));
	else if (LocalEdgeBlockPosition.Y == FChunkData::GetChunkSize(this) - 1)
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
		AdjBlockPosition.X += FChunkData::GetChunkSize(this);
	}
	else if (Position.X >= FChunkData::GetChunkSize(this))
	{
		AdjChunkPosition.X += 1;
		AdjBlockPosition.X -= FChunkData::GetChunkSize(this);
	}
	if (Position.Y < 0)
	{
		AdjChunkPosition.Y -= 1;
		AdjBlockPosition.Y += FChunkData::GetChunkSize(this);
	}
	else if (Position.Y >= FChunkData::GetChunkSize(this))
	{
		AdjChunkPosition.Y += 1;
		AdjBlockPosition.Y -= FChunkData::GetChunkSize(this);
	}

	return true;
}

bool AChunkBase::IsWithinChunkBounds(const FIntVector& Position) const
{
	int32 ChunkSize = FChunkData::GetChunkSize(this);
	return Position.X >= 0 && Position.X < ChunkSize &&
		Position.Y >= 0 && Position.Y < ChunkSize &&
		Position.Z >= 0 && Position.Z < FChunkData::GetChunkHeight(this);
}

bool AChunkBase::IsWithinVerticalBounds(const FIntVector& Position) const
{
	return Position.Z >= 0 && Position.Z < FChunkData::GetChunkHeight(this);
}

void AChunkBase::SpawnBlock(const FIntVector& LocalChunkBlockPosition, EBlock BlockType)
{
 	if (!bCanChangeBlocks) return;

	EBlock CurrentBlockType = GetBlockAtPosition(LocalChunkBlockPosition);
	if (CurrentBlockType != EBlock::Air && CurrentBlockType != EBlock::Water) return;

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
	if (GetBlockAtPosition(GetPositionInDirection(EDirection::Up, LocalChunkBlockPosition)) == EBlock::Water
		|| GetBlockAtPosition(GetPositionInDirection(EDirection::Left, LocalChunkBlockPosition)) == EBlock::Water
		|| GetBlockAtPosition(GetPositionInDirection(EDirection::Right, LocalChunkBlockPosition)) == EBlock::Water
		|| GetBlockAtPosition(GetPositionInDirection(EDirection::Forward, LocalChunkBlockPosition)) == EBlock::Water
		|| GetBlockAtPosition(GetPositionInDirection(EDirection::Backward, LocalChunkBlockPosition)) == EBlock::Water)
	{
		SetBlockAtPosition(LocalChunkBlockPosition, EBlock::Water);
	}
	else
	{
		SetBlockAtPosition(LocalChunkBlockPosition, EBlock::Air);
	}
	
	RegenerateMeshAsync();
}

FChunkMeshData& AChunkBase::GetMeshDataForBlock(EBlock BlockType)
{
	switch (BlockType)
	{
	case EBlock::Water:
		return WaterChunkMeshData;
	case EBlock::OakLeaves:
	case EBlock::BirchLeaves:
		return MaskedChunkMeshData;
	default:
		return OpaqueChunkMeshData;
	}
}

int& AChunkBase::GetVertexCountForBlock(EBlock BlockType)
{
	switch (BlockType)
	{
	case EBlock::Water:
		return WaterVertexCount;
	case EBlock::OakLeaves:
	case EBlock::BirchLeaves:
		return MaskedVertexCount;
	default:
		return OpaqueVertexCount;
	}
}