// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chunk.generated.h"

enum class EDirection;
enum class EBlock;

class UProceduralMeshComponent;
class UFastNoiseWrapper;

UCLASS()
class VOXELGEN_API AChunk : public AActor
{
	GENERATED_BODY()
	
public:
	AChunk();

protected:
	virtual void BeginPlay() override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int ChunkSize = 16;
	
	UPROPERTY(EditAnywhere, Category = "Chunk")
	int Scale = 1;
	
	UPROPERTY(EditAnywhere, Category = "Chunk")
	float Frequency = 0.03;

private:
	TObjectPtr<UProceduralMeshComponent> Mesh;

	TObjectPtr<UFastNoiseWrapper> Noise;

	TArray<EBlock> Blocks;

	TArray<FVector> Verticies;
	TArray<int> Triangles;
	TArray<FVector2D> UV;

	int VertexCount = 0;

	const FVector BlockVerticies[8] = {
		FVector(100,100,100),
		FVector(100,0,100),
		FVector(100,0,0),
		FVector(100,100,0),
		FVector(0,0,100),
		FVector(0,100,100),
		FVector(0,100,0),
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
	
	const int BlockSize = 100;

private:
	void GenerateBlocks();

	void GenerateMesh();

	void ApplyMesh() const;

	bool CheckIsAir(const FVector& Position) const;

	void CreateFace(EDirection Direction, const FVector& Position);

	TArray<FVector> GetFaceVerticies(EDirection Direction, const FVector& Position) const;

	FVector GetPositionInDirection(EDirection Direction, const FVector& Position) const;

	int GetBlockIndex(int X, int Y, int Z) const;
	
};
