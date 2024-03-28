#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <fbxsdk.h>
THIRD_PARTY_INCLUDES_END

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicFBXImporter.generated.h"


UCLASS()
class RUNTIMEGEOMETRYUTILS_API ADynamicFBXImporter : public AActor
{
	GENERATED_BODY()

public:

	ADynamicFBXImporter();
	~ADynamicFBXImporter();

	UPROPERTY(VisibleAnywhere,BlueprintReadWrite)
	class AImportedFBXActor* SpawnedActor;

	UPROPERTY()
	TArray<class UDynamicMeshComponent*> MeshComponents;

	void BuildMesh(FbxNode* node, UE::Geometry::FDynamicMesh3& MeshOut);

	FbxManager* SdkManager;
	FbxIOSettings* ios;
	FbxImporter* importer;

public:

	void IterateNode(FbxNode* node);

	/**
	 * Read mesh in FBX format from the given path into a FDynamicMesh3.
	 * @param bNormals should normals be imported into primary normal attribute overlay
	 * @param bTexCoords should texture coordinates be imported into primary UV attribute overlay
	 * @param bVertexColors should normals be imported into per-vertex colors
	 * @param bReverseOrientation if true, mesh orientation/normals are flipped. You probably want this for importing to UE4 from other apps.
	 * @param return false if read failed
	 */
	UFUNCTION(BlueprintCallable)
	bool ReadFBXMesh(
		const FString& Path,
		bool bNormals,
		bool bTexCoords,
		bool bVertexColors,
		bool bReverseOrientation);

};