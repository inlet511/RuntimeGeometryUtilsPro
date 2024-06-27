// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generators/MeshShapeGenerator.h"

/**
 * 
 */
class RUNTIMEGEOMETRYUTILS_API FDelaunayGenrator: public UE::Geometry::FMeshShapeGenerator
{
public:
	UPROPERTY(VisibleAnywhere,BlueprintReadWrite, Category="Vertices")
	TArray<FVector2f> InputVertices;

	void Triangulate();

	virtual FMeshShapeGenerator& Generate() override;	
};
