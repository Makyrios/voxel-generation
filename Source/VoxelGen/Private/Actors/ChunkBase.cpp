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
    CacheBlockDataTable();
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

	// Create section for Water blocks (Material Slot 1)
	if (WaterChunkMeshData.Vertices.Num() > 0 && WaterMaterial)
	{
		Mesh->CreateMeshSection(1, WaterChunkMeshData.Vertices, WaterChunkMeshData.Triangles,
								TArray<FVector>(), WaterChunkMeshData.UV, WaterChunkMeshData.Colors,
								TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(1, WaterMaterial);
	}

	// Create section for Leaves (masked) (Material Slot 2)
	if (LeavesChunkMeshData.Vertices.Num() > 0 && LeavesMaterial)
	{
		Mesh->CreateMeshSection(2, LeavesChunkMeshData.Vertices, LeavesChunkMeshData.Triangles,
								TArray<FVector>(), LeavesChunkMeshData.UV, LeavesChunkMeshData.Colors,
								TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(2, LeavesMaterial);
	}

	// Create section for Grass (masked) (Material Slot 3)
	if (GrassChunkMeshData.Vertices.Num() > 0 && GrassMaterial)
	{
		Mesh->CreateMeshSection(3, GrassChunkMeshData.Vertices, GrassChunkMeshData.Triangles,
								TArray<FVector>(), GrassChunkMeshData.UV, GrassChunkMeshData.Colors,
								TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(3, GrassMaterial);
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

void AChunkBase::CreateCrossPlanes(const FIntVector& BlockPos, EBlock BlockType, const FBlockSettings& Settings)
{
	if (!ParentWorld) return;
	
	// get mesh buffer & vertex counter
    FChunkMeshData& ChunkMeshData = GetMeshDataForBlock(BlockType);
    int& VertexCount = GetVertexCountForBlock(BlockType);

	// Pick a random texture variant for this block
	int32 Seed = ParentWorld->Seed
		   ^ (BlockPos.X * 73856093)
		   ^ (BlockPos.Y * 19349663)
		   ^ (BlockPos.Z * 83492791);
	FRandomStream Stream(Seed);
	
	int32 NumVariants = Settings.NumTextureVariants;
	int32 VariantIndex = Stream.RandRange(0, NumVariants-1);

    // world space origin of this block
    const float ScaledBlockSize = FChunkData::GetScaledBlockSize(this);
	
    FVector Origin((BlockPos.X + 0.5f) * ScaledBlockSize, (BlockPos.Y + 0.5f) * ScaledBlockSize, BlockPos.Z * ScaledBlockSize);

    // size of the planes
    float HalfWidth = 0.5f * Settings.RenderScale * ScaledBlockSize;
    float Height = Settings.RenderHeight * ScaledBlockSize;

    // Define the four corners of a plane centered at Origin + (0,0,H/2)
    FVector A(-HalfWidth,  0, Height * 0.5f);
    FVector B( HalfWidth,  0, Height * 0.5f);
    FVector C( HalfWidth,  0,    0);
    FVector D(-HalfWidth,  0,    0);

	// Randomize rotation
	if (Settings.RandomRotation)
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

int AChunkBase::GetTextureIndex(EBlock BlockType, const FVector& Normal) const
{
	FBlockSettings Data = GetBlockData(BlockType);

	// Simplified example, assumes your texture array matches this order conceptually
	if (Normal == FVector::UpVector) return Data.TextureData.TopFaceTexture;
	if (Normal == FVector::DownVector) return Data.TextureData.BottomFaceTexture;
	return Data.TextureData.SideFaceTexture;
}

FBlockSettings AChunkBase::GetBlockData(EBlock BlockType) const
{
	if (!BlockDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockDataTable is not set in ChunkBase!"));
		return FBlockSettings();
	}
	
	const FString EnumString = UEnum::GetValueAsString(BlockType);
	const FName RowName = FName(EnumString.RightChop(EnumString.Find(TEXT("::")) + 2));
	
    const FBlockSettings& FoundRow = BlockSettingsCache[BlockType];
    
	return FoundRow;
}

bool AChunkBase::ShouldRenderFace(const FIntVector& Position) const
{
	EBlock Block = GetBlockAtPosition(Position);
	if (Block == EBlock::Air) return true;

	FBlockSettings Data = GetBlockData(Block);
	// A block is considered "air" for culling purposes if it's not solid OR if it's transparent.
	return (!Data.bIsSolid || Data.bIsTransparent);
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
	LeavesChunkMeshData.Clear();
	GrassChunkMeshData.Clear();

	OpaqueVertexCount = 0;
	WaterVertexCount = 0;
	LeavesVertexCount = 0;
	GrassVertexCount = 0;

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
	
	if (auto* FoundChunkPtr = ParentWorld->GetChunksData().Find(AdjChunkPos))
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
		
		// Destroy grass foliage block above the destroyed block
		FIntVector UpBlockPos = GetPositionInDirection(EDirection::Up, LocalChunkBlockPosition);
		if (GetBlockAtPosition(UpBlockPos) == EBlock::GrassFoliage)
		{
			SetBlockAtPosition(UpBlockPos, EBlock::Air);
		}
	}
	
	RegenerateMeshAsync();
}

FChunkMeshData& AChunkBase::GetMeshDataForBlock(EBlock BlockType)
{
	FBlockSettings Settings = GetBlockData(BlockType);
	
	switch (Settings.MaterialType)
	{
	case EBlockMaterialType::Water:
		return WaterChunkMeshData;
	case EBlockMaterialType::Leaves:
		return LeavesChunkMeshData;
	case EBlockMaterialType::Opaque:
		return OpaqueChunkMeshData;
	case EBlockMaterialType::Grass:
		return GrassChunkMeshData;
	default:
		return OpaqueChunkMeshData;
	}
}

int& AChunkBase::GetVertexCountForBlock(EBlock BlockType)
{
	FBlockSettings Settings = GetBlockData(BlockType);
	switch (Settings.MaterialType)
	{
	case EBlockMaterialType::Water:
		return WaterVertexCount;
	case EBlockMaterialType::Leaves:
		return LeavesVertexCount;
	case EBlockMaterialType::Opaque:
		return OpaqueVertexCount;
	case EBlockMaterialType::Grass:
		return GrassVertexCount;
	default:
		return OpaqueVertexCount;
	}
}

void AChunkBase::CacheBlockDataTable()
{
    if (!BlockDataTable) return;
    UEnum* EnumPtr = StaticEnum<EBlock>();
    for (int32 Value = 0; Value < EnumPtr->NumEnums() - 1; ++Value)
    {
        const FString EnumString = EnumPtr->GetNameStringByIndex(Value);
        const FName RowName = FName(EnumString);
        
        if (FBlockSettings* Row = BlockDataTable->FindRow<FBlockSettings>(RowName, TEXT("Caching")))
        {
            BlockSettingsCache.Add(static_cast<EBlock>(Value), *Row);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No DataTable row for block type %s"), *RowName.ToString());
        }
    }
}