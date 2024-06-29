#include "DynamicSDMCActor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MaterialDomain.h"


// Sets default values
ADynamicSDMCActor::ADynamicSDMCActor()
{
	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("MeshComponent"), false);
	SetRootComponent(MeshComponent);
}

// Called when the game starts or when spawned
void ADynamicSDMCActor::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void ADynamicSDMCActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADynamicSDMCActor::GenerateCollision()
{
	MeshComponent->CollisionType = CTF_UseSimpleAndComplex;
	//MeshComponent->EnableComplexAsSimpleCollision();
	//MeshComponent->UpdateCollision(false);
	// 获取ConvexHull
	int32 vertexCount = SourceMesh.VertexCount();
	FConvexHull3f ConvexHull;
	ConvexHull.Solve(vertexCount,[&](int32 vertID)->UE::Math::TVector<float>
	{
		FVector VertexPos = SourceMesh.GetVertex(vertID);
		return FVector3f(VertexPos.X,VertexPos.Y,VertexPos.Z);
	});

	TArray<int32> indices;
	for(auto Tri:ConvexHull.GetTriangles())
	{
		indices.Add(Tri.A);
		indices.Add(Tri.B);
		indices.Add(Tri.C);
	}

	TArray<FVector3d> VertexArray;			
	for(auto V : SourceMesh.VerticesItr())
	{
		VertexArray.Add(V);
	}
	
	FKAggregateGeom AggGeom;
	AggGeom.ConvexElems.Empty();
	FKConvexElem Elem;
	Elem.VertexData = VertexArray;
	Elem.IndexData = indices;
	AggGeom.ConvexElems.Add(Elem);
	MeshComponent->SetSimpleCollisionShapes(AggGeom,true);
}


void ADynamicSDMCActor::OnMeshEditedInternal()
{
	UpdateSDMCMesh();
	Super::OnMeshEditedInternal();
}

void ADynamicSDMCActor::UpdateSDMCMesh()
{
	if (MeshComponent)
	{
		*(MeshComponent->GetMesh()) = SourceMesh;
		MeshComponent->NotifyMeshUpdated();

		// update material on new section
		UMaterialInterface* UseMaterial = (this->Material != nullptr) ? this->Material : UMaterial::GetDefaultMaterial(MD_Surface);
		MeshComponent->SetMaterial(0, UseMaterial);
	}

	if(bGenerateCollision)
	{
		GenerateCollision();
		MeshComponent->SetSimulatePhysics(true);
	}	
	
}