// Fill out your copyright notice in the Description page of Project Settings.


#include "ImportedFBXActor.h"
#include "Components/DynamicMeshComponent.h"
#include "MeshComponentRuntimeUtils.h"


// Sets default values
AImportedFBXActor::AImportedFBXActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AImportedFBXActor::AddMeshComponent(FName MeshName, UE::Geometry::FDynamicMesh3& SourceMesh)
{
	UStaticMeshComponent* NewComponent = NewObject<UStaticMeshComponent>(this, MeshName);

	if (NewComponent)
	{
		UStaticMesh* MyStaticMesh = NewObject<UStaticMesh>();
		NewComponent->SetStaticMesh(MyStaticMesh);

		RTGUtils::UpdateStaticMeshFromDynamicMesh(MyStaticMesh, &SourceMesh);

		NewComponent->RegisterComponent();
		NewComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

		
		// Add to Mesh Array
		MeshComponents.Add(NewComponent);
	}

}

int32 AImportedFBXActor::CountComponents()
{
	return GetComponents().Num();
}

// Called when the game starts or when spawned
void AImportedFBXActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AImportedFBXActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

