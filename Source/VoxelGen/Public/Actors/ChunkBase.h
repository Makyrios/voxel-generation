// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/ChunkMeshData.h"
#include "GameFramework/Actor.h"
#include "Structs/BlockSettings.h"
#include "Structs/ChunkColumn.h"
#include "ChunkBase.generated.h"

enum class EDirection;
enum class EBlock;

class UProceduralMeshComponent;
class UFastNoiseWrapper;
class AChunkWorld;

UCLASS(Abstract)
class VOXELGEN_API AChunkBase : public AActor
{
	GENERATED_BODY()
	
public:
	AChunkBase();
	
	void RegenerateMesh();
	void RegenerateMeshAsync();
	void ClearMesh();

	TArray<FChunkColumn> GetColumns() const { return ChunkColumns; }
	void SetColumns(const TArray<FChunkColumn>& NewColumns);

	void SpawnBlock(const FIntVector& LocalChunkBlockPosition, EBlock BlockType);
	void DestroyBlock(const FIntVector& LocalChunkBlockPosition);
	
	void SetParentWorld(AChunkWorld* World) { ParentWorld = World; }

	// Position is in local chunk coordinates
	EBlock GetBlockAtPosition(const FIntVector& Position) const;
	EBlock GetBlockAtPosition(int X, int Y, int Z) const;
	
	bool IsMeshInitialized() const { return bIsMeshInitialized; }
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void GenerateMesh() PURE_VIRTUAL(&AChunkBase::GenerateMesh);
	void CreateCrossPlanes(
		const FIntVector& BlockPos,
		EBlock BlockType,
		const FBlockSettings& Settings
	);

	int GetTextureIndex(EBlock BlockType, const FVector& Normal) const;
	FBlockSettings GetBlockData(EBlock BlockType) const;

	bool ShouldRenderFace(const FIntVector& Position) const;
	bool ShouldRenderFace(int X, int Y, int Z) const;

	FIntVector GetPositionInDirection(EDirection Direction, const FIntVector& Position) const;
	void SetBlockAtPosition(const FIntVector& Position, EBlock BlockType);

	AChunkBase* GetAdjacentChunk(const FIntVector& Position, FIntVector* const outAdjChunkBlockPosition = nullptr) const;
	bool AdjustForAdjacentChunk(const FIntVector& Position, FIntVector2& AdjChunkPosition, FIntVector& AdjBlockPosition) const;

	FORCEINLINE bool IsWithinChunkBounds(const FIntVector& Position) const;
	FORCEINLINE bool IsWithinVerticalBounds(const FIntVector& Position) const;

	// Update the adjacent chunk if the block is at the edge of the chunk
	void UpdateAdjacentChunk(const FIntVector& LocalEdgeBlockPosition) const;
	TArray<FIntVector> GetEdgeOffsets(const FIntVector& LocalEdgeBlockPosition) const;

	FChunkMeshData& GetMeshDataForBlock(EBlock BlockType);
	int& GetVertexCountForBlock(EBlock BlockType);
    void CacheBlockDataTable();

private:
	void ApplyMesh();

public:
	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	FIntVector2 ChunkPosition;
	
	bool bIsProcessingMesh = false;
	
	FChunkMeshData OpaqueChunkMeshData;
	FChunkMeshData WaterChunkMeshData;
	FChunkMeshData LeavesChunkMeshData;
	FChunkMeshData GrassChunkMeshData;
	
	int OpaqueVertexCount = 0;
	int WaterVertexCount = 0;
	int LeavesVertexCount = 0;
	int GrassVertexCount = 0;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Data")
	TObjectPtr<UDataTable> BlockDataTable;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Chunk|Columns")
	TArray<FChunkColumn> ChunkColumns;
	
	// Material properties
	UPROPERTY(EditAnywhere, Category = "Chunk|Materials")
	TObjectPtr<UMaterialInterface> OpaqueMaterial;

	UPROPERTY(EditAnywhere, Category = "Chunk|Materials")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(EditAnywhere, Category = "Chunk|Materials")
	TObjectPtr<UMaterialInterface> LeavesMaterial;

	UPROPERTY(EditAnywhere, Category = "Chunk|Materials")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	UPROPERTY()
	AChunkWorld* ParentWorld;
	
	TArray<FVector> BlockVerticies;

    TMap<EBlock, FBlockSettings> BlockSettingsCache;
	
	const int BlockTriangles[24] = {
		0,1,2,3, // Forward
		5,0,3,6, // Right
		4,5,6,7, // Back
		1,4,7,2, // Left
		5,4,1,0, // Up
		3,2,7,6  // Down
	};

private:
	bool bIsMeshInitialized = false;
	bool bCanChangeBlocks = true;

	
};
