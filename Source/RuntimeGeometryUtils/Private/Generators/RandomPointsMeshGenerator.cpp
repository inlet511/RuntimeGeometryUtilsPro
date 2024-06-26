﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "../../Public/Generators/RandomPointsMeshGenerator.h"

#include "CompGeom/ConvexHull2.h"
#include "CompGeom/PolygonTriangulation.h"
#include "../Algorithms/PointTriangleRelation.h"

bool FRandomPointsMeshGenerator::GenerateVertices()
{	
	VertexCount = InputVertices.Num();
	if(VertexCount <=3)
		return false;


	UE::Geometry::FConvexHull2f ConvexHull;
	TArray<FVector2f> Array;

	// 构造ConvexHull
	ConvexHull.Solve(InputVertices.Num(),[=,this](int32 i)
	{
		const FVector2f Point(InputVertices[i].X, InputVertices[i].Y);
		return Point;
	});

	// 获取ConvexHull
	ConvexHullIndices = ConvexHull.GetPolygonIndices();
	int32 ConvexHullPointCount = ConvexHullIndices.Num();

	ConvexHullVertices.SetNum(ConvexHullPointCount);
	for(int32 i = 0; i< ConvexHullPointCount; ++i)
	{
		ConvexHullVertices[i] = InputVertices[ConvexHullIndices[i]];
			//FVector2f(InputVertices[ConvexHullIndices[i]].X,InputVertices[ConvexHullIndices[i]].Y);
	}

	// Setup Buffer
	Vertices.SetNum(VertexCount);
	UVs.SetNum(VertexCount);
	UVParentVertex.SetNum(VertexCount);
	Normals.SetNum(VertexCount);
	NormalParentVertex.SetNum(VertexCount);

	for(int32 i = 0; i<VertexCount; ++i)
	{
		Vertices[i] = FVector(InputVertices[i].X, InputVertices[i].Y, 10);
		UVs[i] = FVector2f(Vertices[i].X / 1000.0f, Vertices[i].Y / 1000.0f);
		UVParentVertex[i] = i;
		Normals[i] = FVector3f::UpVector;
		NormalParentVertex[i] = i;
	}

	return true;
}

void FRandomPointsMeshGenerator::Triangulate()
{
	// 注意这里ConvexHullTriangles中的索引是ConvexHullVertices中的索引，不是InputVertices的索引
	PolygonTriangulation::TriangulateSimplePolygon(ConvexHullVertices,ConvexHullTriangles);
	

	for(int i = 0 ; i< VertexCount; ++i)
	{
		// 如果是ConvexHull点则跳过
		if(ConvexHullIndices.Contains(i))
			continue;

		for(int j = 0 ; j< ConvexHullTriangles.Num(); ++j)
		{
			
			//从ConvexHullTriangles中正确取出三个点的索引
			UE::Geometry::FIndex3i CurrentTriangle = UE::Geometry::FIndex3i(ConvexHullIndices[ConvexHullTriangles[j].A],ConvexHullIndices[ConvexHullTriangles[j].B], ConvexHullIndices[ConvexHullTriangles[j].C]);
			// 在三角形内部
			if(PointTriangleRelation::IsPointInTriangle2D(InputVertices[i],InputVertices[CurrentTriangle.A],InputVertices[CurrentTriangle.B], InputVertices[CurrentTriangle.C]))
			{
				// 创建三个新三角形
				UE::Geometry::FIndex3i Tri1(CurrentTriangle.A, CurrentTriangle.B, i);
				UE::Geometry::FIndex3i Tri2(CurrentTriangle.B, CurrentTriangle.C, i);
				UE::Geometry::FIndex3i Tri3(CurrentTriangle.C, CurrentTriangle.A,i);

				// ConvexHullTriangles中移除当前这个三角形
				ConvexHullTriangles.RemoveAt(j);
				Triangles.Add(Tri1);
				Triangles.Add(Tri2);
				Triangles.Add(Tri3);
				break;
			}
		}
	}

	// 添加还没有被“拆分”的ConvexHullTriangles
	for(int32 k = 0; k< ConvexHullTriangles.Num(); ++k)
	{
		UE::Geometry::FIndex3i Tri(ConvexHullIndices[ConvexHullTriangles[k].A], ConvexHullIndices[ConvexHullTriangles[k].B],ConvexHullIndices[ConvexHullTriangles[k].C]);
		Triangles.Add(Tri);
	}

	// 设置Triangle其他几个数组
	int32 TriangleCount = Triangles.Num();
	TriangleUVs.SetNum(TriangleCount);
	TriangleNormals.SetNum(TriangleCount);
	TrianglePolygonIDs.SetNum(TriangleCount);

	for(int32 i = 0; i<TriangleCount; ++i)
	{
		SetTriangleUVs(i,Triangles[i]);
		SetTriangleNormals(i, Triangles[i]);
		SetTrianglePolygon(i,0);
	}
}

UE::Geometry::FMeshShapeGenerator& FRandomPointsMeshGenerator::Generate()
{
	if(GenerateVertices())
	{
		Triangulate();
	}
	return *this;
}