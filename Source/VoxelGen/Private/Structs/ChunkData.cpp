#include "Structs/ChunkData.h"
#include "GameInstances/WorldGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// Helper function (optional, put inside cpp)
namespace {
    UWorldGameInstance* GetWorldGI(const UObject* WorldContext) {
        if (!WorldContext) return nullptr;
        UWorld* World = WorldContext->GetWorld();
        if (!World) return nullptr;
        return Cast<UWorldGameInstance>(World->GetGameInstance());
    }
}

int32 FChunkData::GetWorldSize(const UObject* WorldContext) {
    UWorldGameInstance* GI = GetWorldGI(WorldContext);
    return GI ? GI->WorldSize : 0;
}

int32 FChunkData::GetWorldSizeInColumns(const UObject* WorldContext) {
    return GetWorldSize(WorldContext) * GetChunkSize(WorldContext);
}

int32 FChunkData::GetChunkSize(const UObject* WorldContext) {
    UWorldGameInstance* GI = GetWorldGI(WorldContext);
    return GI ? GI->ChunkSize : 0;
}

int32 FChunkData::GetChunkHeight(const UObject* WorldContext) {
    UWorldGameInstance* GI = GetWorldGI(WorldContext);
    return GI ? GI->ChunkHeight : 0;
}

float FChunkData::GetBlockSize(const UObject* WorldContext) {
     UWorldGameInstance* GI = GetWorldGI(WorldContext);
     return GI ? GI->BlockSize : 0;
 }

 float FChunkData::GetBlockScale(const UObject* WorldContext) {
      UWorldGameInstance* GI = GetWorldGI(WorldContext);
      return GI ? GI->BlockScale : 0;
  }

 float FChunkData::GetScaledBlockSize(const UObject* WorldContext) {
     return GetBlockSize(WorldContext) * GetBlockScale(WorldContext);
 }

int32 FChunkData::GetBlockIndex(const UObject* WorldContext, int32 X, int32 Y, int32 Z) {
    int32 Size = GetChunkSize(WorldContext);
    int32 Height = GetChunkHeight(WorldContext);
    if (X < 0 || X >= Size || Y < 0 || Y >= Size || Z < 0 || Z >= Height) return -1;
    return X + (Y * Size) + (Z * Size * Size);
}

 int32 FChunkData::GetColumnIndex(const UObject* WorldContext, int32 X, int32 Y) {
     int32 Size = GetChunkSize(WorldContext);
     if (X < 0 || X >= Size || Y < 0 || Y >= Size) return -1;
     return X + (Y * Size);
 }

int32 FChunkData::GetColumnIndexFromLocal(int32 X, int32 Y, int32 ChunkSize)
{
    return X + (Y * ChunkSize);
}

FIntVector FChunkData::GetLocalBlockPosition(const UObject* WorldContext, const FIntVector& WorldBlockPosition) {
     int32 Size = GetChunkSize(WorldContext);
     int32 Height = GetChunkHeight(WorldContext);
     return FIntVector(
         (WorldBlockPosition.X % Size + Size) % Size,
         (WorldBlockPosition.Y % Size + Size) % Size,
         (WorldBlockPosition.Z % Height + Height) % Height
     );
 }

  FIntVector FChunkData::GetWorldBlockPosition(const UObject* WorldContext, const FVector& WorldPosition) {
      float ScaledSize = GetScaledBlockSize(WorldContext);
      if (FMath::IsNearlyZero(ScaledSize)) return FIntVector::ZeroValue;
      return FIntVector(
          FMath::FloorToInt(WorldPosition.X / ScaledSize),
          FMath::FloorToInt(WorldPosition.Y / ScaledSize),
          FMath::FloorToInt(WorldPosition.Z / ScaledSize)
      );
  }

 FIntVector2 FChunkData::GetChunkContainingBlockPosition(const UObject* WorldContext, const FIntVector& WorldBlockPosition) {
     int32 Size = GetChunkSize(WorldContext);
      if (Size == 0) return FIntVector2::ZeroValue;
     return FIntVector2(
          FMath::FloorToInt(static_cast<float>(WorldBlockPosition.X) / Size),
          FMath::FloorToInt(static_cast<float>(WorldBlockPosition.Y) / Size)
     );
 }

  FIntVector2 FChunkData::GetChunkPosition(const UObject* WorldContext, const FVector& WorldPosition) {
      return GetChunkContainingBlockPosition(WorldContext, GetWorldBlockPosition(WorldContext, WorldPosition));
  }