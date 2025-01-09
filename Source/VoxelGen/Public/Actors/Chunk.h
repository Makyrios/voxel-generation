// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structs/ChunkMeshData.h"
#include "GameFramework/Actor.h"
#include "Structs/ChunkData.h"
#include "Chunk.generated.h"

enum class EDirection;
enum class EBlock;

class UProceduralMeshComponent;
class UTerrainGenerator;
class UFastNoiseWrapper;
class AChunkWorld;

UCLASS()
class VOXELGEN_API AChunk : public AActor
{
	GENERATED_BODY()
	
public:
	AChunk();
	
	void RegenerateMesh();
	void RegenerateMeshAsync();
	void ClearMesh();

	void GenerateBlocks(const FVector& ChunkWorldPosition);
	
	void SpawnBlock(const FIntVector& LocalChunkBlockPosition, EBlock BlockType);
	
	void DestroyBlock(const FIntVector& LocalChunkBlockPosition);
	
	void SetParentWorld(AChunkWorld* World) { ParentWorld = World; }
	
	EBlock GetBlockAtPosition(const FIntVector& Position) const;

	bool IsMeshInitialized() const { return bIsMeshInitialized; }


protected:
	virtual void BeginPlay() override;

private:
	void GenerateMesh();
	void ApplyMesh();
	void CreateFace(EDirection Direction, const FIntVector& Position);
	TArray<FVector> GetFaceVerticies(EDirection Direction, const FVector& WorldPosition) const;
	FIntVector GetPositionInDirection(EDirection Direction, const FIntVector& Position) const;
	
	bool CheckIsAir(const FIntVector& Position) const;
	
	void SetBlockAtPosition(const FIntVector& Position, EBlock BlockType);
	
	AChunk* GetAdjacentChunk(const FIntVector& Position, FIntVector* const outAdjChunkBlockPosition = nullptr) const;
	bool AdjustForAdjacentChunk(const FIntVector& Position, FIntVector2& AdjChunkPosition, FIntVector& AdjBlockPosition) const;

	bool IsWithinChunkBounds(const FIntVector& Position) const;
	bool IsWithinVerticalBounds(const FIntVector& Position) const;

	// Update the adjacent chunk if the block is at the edge of the chunk
	void UpdateAdjacentChunk(const FIntVector& LocalEdgeBlockPosition) const;
	TArray<FIntVector> GetEdgeOffsets(const FIntVector& LocalEdgeBlockPosition) const;

	int GetTextureIndex(EBlock BlockType, EDirection Direction) const;

public:
	FIntVector2 ChunkPosition;
	bool bIsProcessingMesh = false;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UTerrainGenerator> TerrainGenerator;

	bool bCanChangeBlocks = true;
	bool bIsMeshInitialized = false;
	
	UPROPERTY(EditAnywhere, Category = "Chunk")
	TObjectPtr<UMaterial> Material;
	
	UPROPERTY(EditAnywhere, Category = "Chunk")
	float Frequency = 0.03;

	UPROPERTY()
	AChunkWorld* ParentWorld;

	TArray<EBlock> Blocks;

	FChunkMeshData ChunkMeshData;
	int VertexCount = 0;

	const FVector BlockVerticies[8] = {
		FVector(FChunkData::BlockSize,FChunkData::BlockSize,FChunkData::BlockSize),
		FVector(FChunkData::BlockSize,0,FChunkData::BlockSize),
		FVector(FChunkData::BlockSize,0,0),
		FVector(FChunkData::BlockSize,FChunkData::BlockSize,0),
		FVector(0,0,FChunkData::BlockSize),
		FVector(0,FChunkData::BlockSize,FChunkData::BlockSize),
		FVector(0,FChunkData::BlockSize,0),
		FVector(0,0,0)
	};
	
	const int BlockTriangles[24] = {
		0,1,2,3, // Forward
		5,0,3,6, // Right
		4,5,6,7, // Back
		1,4,7,2, // Left
		5,4,1,0, // Up
		3,2,7,6  // Down
	};
	
};
