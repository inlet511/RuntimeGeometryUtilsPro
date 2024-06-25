// Fill out your copyright notice in the Description page of Project Settings.


#include "Generators/TriangulationGenerator.h"
#include "CompGeom/ConvexHull2.h"
#include "CompGeom/PolygonTriangulation.h"


FConvexHullGenerator::FConvexHullGenerator()
{
}

FConvexHullGenerator::~FConvexHullGenerator()
{
}

void FConvexHullGenerator::GenerateVertices()
{
	UE::Geometry::FConvexHull2f ConvexHull;
	TArray<FVector2f> Array;
	
	ConvexHull.SolveSimplePolygon(OriginalVertices.Num(),[=](int32 i)
	{
		const FVector2f Point(OriginalVertices[i].X, OriginalVertices[i].Y);
		return Point;
	});

	// 获取到ConvexHull
	int32 ConvexHullPointCount = ConvexHull.GetNumUniquePoints();
	TArray<int32> ConvexHullIndices = ConvexHull.GetPolygonIndices();

	for(int32 i = 0; i< ConvexHullPointCount; i++)
	{
		FVector2f Vertex = OriginalVertices[ConvexHullIndices[i]];
		Vertices[i] = FVector(Vertex.X, Vertex.Y, 10);
		UVs[i] = FVector2f(Vertex.X / 1000.0f, Vertex.Y/1000.0f);
		UVParentVertex[i] = i;
		Normals[i] = FVector3f(0,0,1);
		NormalParentVertex[i] = i;
	}
}


void FConvexHullGenerator::OutputTriangles()
{
	TArray<UE::Geometry::FIndex3i> indices;
	PolygonTriangulation::TriangulateSimplePolygon(Vertices,indices);
	int32 TriangleCount = indices.Num() / 3;
	for(int i = 0; i< TriangleCount; ++i)
	{
		SetTriangle(i,i*3,i+3+1,i*3+2);
	}
}

UE::Geometry::FMeshShapeGenerator& FConvexHullGenerator::Generate()
{
	

	
	
	return *this;
}
