// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkWorld.generated.h"

class AChunk;

UCLASS()
class VOXELGEN_API AChunkWorld : public AActor
{
	GENERATED_BODY()
	
public:	
	AChunkWorld();

	const TMap<FVector2D, AChunk*>& GetChunksData() { return ChunksData; }

protected:
	virtual void BeginPlay() override;

private:
	void CreateChunks();

public:
	virtual void Tick(float DeltaTime) override;

private:
	TMap<FVector2D, AChunk*> ChunksData;
	
	UPROPERTY(EditDefaultsOnly, Category = "Chunk World")
	TSubclassOf<AChunk> ChunkClass;
	
};
