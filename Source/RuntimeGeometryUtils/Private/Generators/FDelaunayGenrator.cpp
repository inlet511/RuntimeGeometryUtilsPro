// Fill out your copyright notice in the Description page of Project Settings.


#include "../../Public/Generators/FDelaunayGenrator.h"
#include "CompGeom/Delaunay2.h"


void FDelaunayGenrator::Triangulate()
{
	int32 VertexCount = InputVertices.Num();
	if(VertexCount<3)
		return;
	
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
		SetTriangle(i, UE::Geometry::FIndex3i(OutTriangles[i].C,OutTriangles[i].B,OutTriangles[i].A));
		SetTriangleNormals(i, Triangles[i]);
		SetTriangleUVs(i,Triangles[i]);
		SetTrianglePolygon(i,0);
	}
	
}

UE::Geometry::FMeshShapeGenerator& FDelaunayGenrator::Generate()
{
	Triangulate();	
	return *this;
}
