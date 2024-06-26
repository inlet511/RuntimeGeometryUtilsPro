// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generators/MeshShapeGenerator.h"

/**
 * 
 */
class RUNTIMEGEOMETRYUTILS_API FRandomPointsMeshGenerator : UE::Geometry::FMeshShapeGenerator
{
public:

	UPROPERTY(VisibleAnywhere,BlueprintReadWrite, Category="Vertices")
	TArray<FVector2f> InputVertices;

	bool GenerateVertices();

	void Triangulate();

	virtual FMeshShapeGenerator& Generate() override;	
	
	// 总点数
	int32 VertexCount;

	// ConvexHull三角形 - 索引存储的是 ConvexHullVertices的索引
	TArray<UE::Geometry::FIndex3i> Triangles2D;
	
	// ConvexHull的顶点
	TArray<FVector2f> ConvexHullVertices;

	// ConvexHull 顶点的索引
	TArray<int32> ConvexHullIndices;
	
};
