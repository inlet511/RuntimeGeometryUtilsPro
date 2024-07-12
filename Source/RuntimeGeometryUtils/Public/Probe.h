// Coded by An Ning in 2024

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Probe.generated.h"

UCLASS()
class RUNTIMEGEOMETRYUTILS_API AProbe : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AProbe();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Value;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
