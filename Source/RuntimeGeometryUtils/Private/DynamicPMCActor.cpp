#include "DynamicPMCActor.h"
#include "MeshComponentRuntimeUtils.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MaterialDomain.h"
#include "Chaos/Deformable/ChaosDeformableCollisionsProxy.h"
#include "CompGeom/ConvexHull3.h"

UE_DISABLE_OPTIMIZATION

// Sets default values
ADynamicPMCActor::ADynamicPMCActor()
{
	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"), false);
	SetRootComponent(MeshComponent);
}

// Called when the game starts or when spawned
void ADynamicPMCActor::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ADynamicPMCActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void ADynamicPMCActor::OnMeshEditedInternal()
{
	// 关闭ComplexAsSimple
	UpdatePMCMesh();
	Super::OnMeshEditedInternal();
}

void ADynamicPMCActor::GenerateCollision()
{
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
	for(auto i: indices)
	{
		VertexArray.Add(SourceMesh.GetVertex(i));
	}

	MeshComponent->ClearCollisionConvexMeshes();
	MeshComponent->AddCollisionConvexMesh(VertexArray);	
}

void ADynamicPMCActor::UpdatePMCMesh()
{
	if (MeshComponent)
	{
		bool bUseFaceNormals = (this->NormalsMode == EDynamicMeshActorNormalsMode::FaceNormals);
		bool bUseUV0 = true;
		bool bUseVertexColors = false;

		bool bGenerateSectionCollision = false;

		// 生成碰撞
		if(bGenerateCollision)
		{
			GenerateCollision();
			MeshComponent->SetSimulatePhysics(true);
		}

		RTGUtils::UpdatePMCFromDynamicMesh_SplitTriangles(MeshComponent, &SourceMesh, bUseFaceNormals, bUseUV0, bUseVertexColors, bGenerateSectionCollision);

		// update material
		MeshComponent->SetMaterial(0, this->Material);		
		MeshComponent->SetMaterial(1, CutPlaneMaterial);		
	}
}

UE_ENABLE_OPTIMIZATION