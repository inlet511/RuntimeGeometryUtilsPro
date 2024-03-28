// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "ImportedFBXActor.generated.h"

using namespace UE::Geometry;

UCLASS()
class RUNTIMEGEOMETRYUTILS_API AImportedFBXActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AImportedFBXActor();

	void AddMeshComponent(FName MeshName, FDynamicMesh3& SourceMesh);

	UPROPERTY(VisibleAnywhere)
	TArray<class UStaticMeshComponent*> MeshComponents;

	UFUNCTION(BlueprintCallable)
	int32 CountComponents();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
