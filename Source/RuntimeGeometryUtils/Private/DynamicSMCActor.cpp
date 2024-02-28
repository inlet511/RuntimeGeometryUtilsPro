#include "DynamicSMCActor.h"
#include "MeshComponentRuntimeUtils.h"
#include "MaterialDomain.h"

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
}