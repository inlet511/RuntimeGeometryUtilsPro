#include "DynamicSMCActor.h"
#include "MeshComponentRuntimeUtils.h"
#include "MaterialDomain.h"
#include "Physics/CollisionPropertySets.h"

// Sets default values
ADynamicSMCActor::ADynamicSMCActor()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"), false);
	SetRootComponent(MeshComponent);
	MyStaticMesh = nullptr;
}

UStaticMesh* ADynamicSMCActor::GetMyStaticMesh()
{
	return MyStaticMesh;
}

// Called when the game starts or when spawned
void ADynamicSMCActor::BeginPlay()
{
	MyStaticMesh = nullptr;
	Super::BeginPlay();
}

void ADynamicSMCActor::PostLoad()
{
	MyStaticMesh = nullptr;
	Super::PostLoad();
}

void ADynamicSMCActor::PostActorCreated()
{
	MyStaticMesh = nullptr;
	Super::PostActorCreated();
}

// Called every frame
void ADynamicSMCActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void ADynamicSMCActor::OnMeshEditedInternal()
{
	UpdateSMCMesh();
	Super::OnMeshEditedInternal();
}

void ADynamicSMCActor::GenerateCollision()
{
	// 获取ConvexHull
	int32 vertexCount = SourceMesh.VertexCount();

	TArray<FVector3d> VertexArray;			

	for(auto V: SourceMesh.VerticesItr())
	{
		VertexArray.Add(V);
	}
	
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

	FKConvexElem Elem;
	Elem.VertexData = VertexArray;
	Elem.IndexData = indices;

	FKAggregateGeom AggGeom;
	AggGeom.ConvexElems.Add(Elem);
	
	UBodySetup* BodySetup = MeshComponent->GetBodySetup();
	BodySetup->Modify();
	BodySetup->RemoveSimpleCollision();
	BodySetup->AggGeom = AggGeom;
	BodySetup->CollisionTraceFlag = (ECollisionTraceFlag)(int32)ECollisionGeometryMode::SimpleAndComplex;
	BodySetup->CreatePhysicsMeshes();

	MeshComponent->UpdateCollisionFromStaticMesh();
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); 	
}

void ADynamicSMCActor::UpdateSMCMesh()
{
	if (MyStaticMesh == nullptr)
	{
		MyStaticMesh = NewObject<UStaticMesh>();
		MeshComponent->SetStaticMesh(MyStaticMesh);
		// add one material slot
		MyStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	}

	if (MeshComponent)
	{
		RTGUtils::UpdateStaticMeshFromDynamicMesh(MyStaticMesh, &SourceMesh);

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
