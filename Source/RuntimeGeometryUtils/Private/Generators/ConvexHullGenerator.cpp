// Fill out your copyright notice in the Description page of Project Settings.


#include "..\..\Public\Generators\ConvexHullGenerator.h"
#include "CompGeom/ConvexHull2.h"
#include "CompGeom/PolygonTriangulation.h"

void FConvexHullGenerator::FindConvexHull()
{
	UE::Geometry::FConvexHull2f ConvexHull;
	TArray<FVector2f> Array;

	// 构造ConvexHull
	ConvexHull.Solve(InputVertices.Num(),[=,this](int32 i)
	{
		const FVector2f Point(InputVertices[i].X, InputVertices[i].Y);
		return Point;
	});

	// 获取到ConvexHull
	ConvexHullIndices = ConvexHull.GetPolygonIndices();
	ConvexHullPointCount = ConvexHullIndices.Num();

	// 设置VertexBuffer
	if(ConvexHullPointCount > 3)
	{
		Vertices.SetNum(ConvexHullPointCount);
		UVs.SetNum(ConvexHullPointCount);
		UVParentVertex.SetNum(ConvexHullPointCount);
		Normals.SetNum(ConvexHullPointCount);
		NormalParentVertex.SetNum(ConvexHullPointCount);		
	}
}

void FConvexHullGenerator::TriangulateConvexHull()
{
	// 确保已经填充了Vertices,这一步会自动填充Triangles
	PolygonTriangulation::TriangulateSimplePolygon(Vertices,Triangles);
	TriangleCount = Triangles.Num();

	// 设置三角形Buffer
	if(TriangleCount>0)
	{
		Triangles.SetNum(TriangleCount);
		TriangleUVs.SetNum(TriangleCount);
		TriangleNormals.SetNum(TriangleCount);
		TrianglePolygonIDs.SetNum(TriangleCount);
	}
}

void FConvexHullGenerator::GenerateVertices()
{
	for(int32 i = 0; i< ConvexHullPointCount; i++)
	{
		FVector2f Vertex = InputVertices[ConvexHullIndices[i]];
		Vertices[i] = FVector(Vertex.X, Vertex.Y, 10);
		UVs[i] = FVector2f(Vertex.X / 1000.0f, Vertex.Y/1000.0f);
		UVParentVertex[i] = i;
		Normals[i] = FVector3f(0,0,1);
		NormalParentVertex[i] = i;
	}
}

void FConvexHullGenerator::OutputTriangles()
{
	for(int i = 0; i< TriangleCount; ++i)
	{
		SetTrianglePolygon(i,0);
		SetTriangleUVs(i,Triangles[i]);
		SetTriangleNormals(i,Triangles[i]);
	}
}

UE::Geometry::FMeshShapeGenerator& FConvexHullGenerator::Generate()
{
	FindConvexHull();
	GenerateVertices();
	TriangulateConvexHull();
	OutputTriangles();
	
	return *this;
}
