// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generators/MeshShapeGenerator.h"

/**
 * 
 */
class RUNTIMEGEOMETRYUTILS_API FConvexHullGenerator : UE::Geometry::FMeshShapeGenerator
{
public:
	// 在一个平面上的一组点
	UPROPERTY(VisibleAnywhere,BlueprintReadWrite, Category="Vertices")
	TArray<FVector2f> InputVertices;

	void FindConvexHull();
	void TriangulateConvexHull();
	void GenerateVertices();
	void OutputTriangles();
	
	
	virtual FMeshShapeGenerator& Generate() override;

	// ConvexHull 边界点数量
	int32 ConvexHullPointCount;
	
	// ConvexHull 顶点的索引
	TArray<int32> ConvexHullIndices;
	
	int32 TriangleCount;
};
