// Fill out your copyright notice in the Description page of Project Settings.


#include "../../Public/Generators/DelaunayTriangulationGenrator.h"
#include "CompGeom/ConvexHull2.h"
#include "CompGeom/PolygonTriangulation.h"
#include "../../Public/Algorithms/PointTriangleRelation.h"
#include "CompGeom/Delaunay2.h"

bool DelaunayTriangulationGenrator::GenerateVertices()
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

void DelaunayTriangulationGenrator::Triangulate()
{
	// 注意这里Triangles中的索引是ConvexHullVertices中的索引，不是InputVertices的索引
	PolygonTriangulation::TriangulateSimplePolygon(ConvexHullVertices,Triangles2D);
	
	// 转换为原始点的索引
	for(int32 i = 0; i< Triangles2D.Num() ; ++i)
	{
		Triangles2D[i].A = ConvexHullIndices[Triangles2D[i].A];
		Triangles2D[i].B = ConvexHullIndices[Triangles2D[i].B];
		Triangles2D[i].C = ConvexHullIndices[Triangles2D[i].C];
	}

	for(int i = 0 ; i< VertexCount; ++i)
	{
		// 如果是ConvexHull点则跳过
		if(ConvexHullIndices.Contains(i))
			continue;

		for(int j = 0 ; j< Triangles2D.Num(); ++j)
		{			
			// 在三角形内部
			if(PointTriangleRelation::IsPointInTriangle2D(InputVertices[i],InputVertices[Triangles2D[j].A],InputVertices[Triangles2D[j].B], InputVertices[Triangles2D[j].C]))
			{
				// 创建三个新三角形
				UE::Geometry::FIndex3i Tri1(Triangles2D[j].A, Triangles2D[j].B, i);
				UE::Geometry::FIndex3i Tri2(Triangles2D[j].B, Triangles2D[j].C, i);
				UE::Geometry::FIndex3i Tri3(Triangles2D[j].C, Triangles2D[j].A,i);

				// ConvexHullTriangles中移除当前这个三角形
				Triangles2D.RemoveAt(j);
				Triangles2D.Add(Tri1);
				Triangles2D.Add(Tri2);
				Triangles2D.Add(Tri3);
				break;
			}
		}
	}

	// 德洛内
	UE::Geometry::FDelaunay2 Delaunay;
	bool bSuccess = Delaunay.Triangulate(InputVertices);
	TArray<UE::Geometry::FIndex3i> OutTriangles;
	if(bSuccess)
	{
		OutTriangles = Delaunay.GetTriangles();		
	}
	
	Vertices.SetNum(VertexCount);
	UVs.SetNum(VertexCount);
	UVParentVertex.SetNum(VertexCount);
	Normals.SetNum(VertexCount);
	NormalParentVertex.SetNum(VertexCount);

	for(int32 i = 0 ; i<VertexCount ; ++i)
	{
		Vertices[i] = FVector3d(InputVertices[i].X, InputVertices[i].Y, 20);
		UVs[i] = FVector2f(InputVertices[i].X / 1000.0f,InputVertices[i].Y/1000.0f);
		UVParentVertex[i] = i;
		Normals[i] = FVector3f::UpVector;
		NormalParentVertex[i] = i;		
	}

	int32 TrianglesCount = OutTriangles.Num();
	Triangles.SetNum(TrianglesCount);
	TriangleUVs.SetNum(TrianglesCount);
	TriangleNormals.SetNum(TrianglesCount);
	TrianglePolygonIDs.SetNum(TrianglesCount);

	
	for(int32 i = 0 ; i< OutTriangles.Num() ; ++i)
	{
		Triangles[i]  = UE::Geometry::FIndex3i(OutTriangles[i].A,OutTriangles[i].B,OutTriangles[i].C);
		SetTriangle(i, UE::Geometry::FIndex3i(OutTriangles[i].A,OutTriangles[i].B,OutTriangles[i].C));
		SetTriangleNormals(i, Triangles[i]);
		SetTriangleUVs(i,Triangles[i]);
		SetTrianglePolygon(i,0);
	}
	
	
}

UE::Geometry::FMeshShapeGenerator& DelaunayTriangulationGenrator::Generate()
{
	if(GenerateVertices())
	{
		Triangulate();
	}
	return *this;
}
