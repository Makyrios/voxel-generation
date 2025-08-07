// Fill out your copyright notice in the Description page of Project Settings.


#include "Structs/ChunkMeshData.h"

void FChunkMeshData::Clear()
{
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UV.Empty();
	Colors.Empty();
}
