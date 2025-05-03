// Fill out your copyright notice in the Description page of Project Settings.


#include "Structs/ChunkData.h"

#include "GameInstances/WorldGameInstance.h"

int32 FChunkData::GetBlockIndex(const UObject* WorldContext, int32 X, int32 Y, int32 Z)
{
	if (X < 0 || X >= GetChunkSize(WorldContext) || Y < 0 || Y >= GetChunkSize(WorldContext) || Z < 0 || Z >= GetChunkHeight(WorldContext))
	{
		return -1;
	}
	return X + (Y * GetChunkSize(WorldContext)) + (Z * GetChunkSize(WorldContext) * GetChunkSize(WorldContext));
}

int32 FChunkData::GetColumnIndex(const UObject* WorldContext, int32 X, int32 Y)
{
	if (X < 0 || X >= GetChunkSize(WorldContext) || Y < 0 || Y >= GetChunkSize(WorldContext))
	{
		return -1;
	}
	return X + (Y * GetChunkSize(WorldContext));
}

FIntVector FChunkData::GetLocalBlockPosition(const UObject* WorldContext, const FIntVector& WorldBlockPosition)
{
	return FIntVector(
		(WorldBlockPosition.X % GetChunkSize(WorldContext) + GetChunkSize(WorldContext)) % GetChunkSize(WorldContext),
		(WorldBlockPosition.Y % GetChunkSize(WorldContext) + GetChunkSize(WorldContext)) % GetChunkSize(WorldContext),
		(WorldBlockPosition.Z % GetChunkHeight(WorldContext) + GetChunkHeight(WorldContext)) % GetChunkHeight(WorldContext)
		);
}

FIntVector FChunkData::GetWorldBlockPosition(const UObject* WorldContext, const FVector& WorldPosition)
{
	FVector BlockPosition3D = WorldPosition / (GetScaledBlockSize(WorldContext));
	return FIntVector(
		FMath::FloorToInt(BlockPosition3D.X),
		FMath::FloorToInt(BlockPosition3D.Y),
		FMath::FloorToInt(BlockPosition3D.Z)
	);
}

FIntVector2 FChunkData::GetChunkContainingBlockPosition(const UObject* WorldContext,
                                                        const FIntVector& WorldBlockPosition)
{
	FIntVector2 ChunkContainingBlock(WorldBlockPosition.X / GetChunkSize(WorldContext), WorldBlockPosition.Y / GetChunkSize(WorldContext));
	if (WorldBlockPosition.X < 0)
	{
		ChunkContainingBlock.X -= 1;
	}
	if (WorldBlockPosition.Y < 0)
	{
		ChunkContainingBlock.Y -= 1;
	}
	return ChunkContainingBlock;
}

FIntVector2 FChunkData::GetChunkPosition(const UObject* WorldContext, const FVector& WorldPosition)
{
	return GetChunkContainingBlockPosition(WorldContext, GetWorldBlockPosition(WorldContext, WorldPosition));
}

int32 FChunkData::GetWorldSize(const UObject* WorldContext)
{
	if (!WorldContext) return 32;
	
	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (UWorldGameInstance* MyGI = Cast<UWorldGameInstance>(GI))
	{
		return MyGI->WorldSize;
	}
	return 32;
}

int32 FChunkData::GetWorldSizeInColumns(const UObject* WorldContext)
{
	return GetWorldSize(WorldContext) * GetChunkSize(WorldContext);
}

int32 FChunkData::GetChunkSize(const UObject* WorldContext)
{
	if (!WorldContext) return 16;
	
	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (UWorldGameInstance* MyGI = Cast<UWorldGameInstance>(GI))
	{
		return MyGI->ChunkSize;
	}
	return 16;
}

int32 FChunkData::GetChunkHeight(const UObject* WorldContext)
{
	if (!WorldContext) return 128;
	
	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (UWorldGameInstance* MyGI = Cast<UWorldGameInstance>(GI))
	{
		return MyGI->ChunkHeight;
	}
	return 128;
}

float FChunkData::GetBlockSize(const UObject* WorldContext)
{
	if (!WorldContext) return 100.f;
	
	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (UWorldGameInstance* MyGI = Cast<UWorldGameInstance>(GI))
	{
		return MyGI->BlockSize;
	}
	return 100.f;
}

float FChunkData::GetBlockScale(const UObject* WorldContext)
{
	if (!WorldContext) return 0.5f;
	
	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (UWorldGameInstance* MyGI = Cast<UWorldGameInstance>(GI))
	{
		return MyGI->BlockScale;
	}
	return 0.5f;
}

float FChunkData::GetScaledBlockSize(const UObject* WorldContext)
{
	return GetBlockSize(WorldContext) * GetBlockScale(WorldContext);
}

float FChunkData::GetWaterThreshold(const UObject* WorldContext)
{
	if (!WorldContext) return 10.f;

	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (UWorldGameInstance* MyGI = Cast<UWorldGameInstance>(GI))
	{
		return MyGI->WaterThreshold;
	}
	return 10.f;
}




