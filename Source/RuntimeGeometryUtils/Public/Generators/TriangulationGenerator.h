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
	FConvexHullGenerator();
	~FConvexHullGenerator();

	// 在一个平面上的一组点
	TArray<FVector2f> OriginalVertices;

	void GenerateVertices();
	void GenerateUV();
	void OutputTriangles();
	
	virtual FMeshShapeGenerator& Generate() override;
};
