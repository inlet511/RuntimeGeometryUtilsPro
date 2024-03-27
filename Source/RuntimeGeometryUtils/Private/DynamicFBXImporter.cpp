#include "DynamicFBXImporter.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Components/DynamicMeshComponent.h"


UE_DISABLE_OPTIMIZATION

using namespace UE::Geometry;


ADynamicFBXImporter::ADynamicFBXImporter()
{
	SdkManager = FbxManager::Create();

	ios = FbxIOSettings::Create(SdkManager, IOSROOT);
	SdkManager->SetIOSettings(ios);

	importer = FbxImporter::Create(SdkManager, "");
}

ADynamicFBXImporter::~ADynamicFBXImporter()
{
	SdkManager->Destroy();
	SdkManager = nullptr;
}

void ADynamicFBXImporter::CreateMesh(FbxNode* node, FDynamicMesh3& MeshOut)
{
	FbxMesh* fbx_mesh = node->GetMesh();
	fbx_mesh->vertices
}

void ADynamicFBXImporter::IterateNode(FbxNode* node)
{
	// Handle this node's attribute
	FbxNodeAttribute* node_attribute = node->GetNodeAttribute();

	if (node_attribute)
	{
		FbxNodeAttribute::EType type = node_attribute->GetAttributeType();
		const char* nodeName = node->GetName();
		FString str = UTF8_TO_TCHAR(nodeName);

		switch (type)
		{
		case FbxNodeAttribute::eMesh:
			UE_LOG(LogTemp, Warning, TEXT("[Mesh] %s"), *str);
			UDynamicMeshComponent* CurrentMeshComponent = SpawnedActor->AddComponentByClass<UDynamicMeshComponent>();

			break;

		case FbxNodeAttribute::eNull:
			UE_LOG(LogTemp, Warning, TEXT("[Null] %s"), *str);
			break;

		case FbxNodeAttribute::eUnknown:
			UE_LOG(LogTemp, Warning, TEXT("[Unknown] %s"), *str);
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("[Other] %s"), *str);
			break;
		}

	}


	// Iterate over children
	int count = node->GetChildCount();
	for (int i = 0; i < count; i++)
	{
		IterateNode(node->GetChild(i));
	}
}



bool ADynamicFBXImporter::ReadFBXMesh(const FString& Path,bool bNormals, bool bTexCoords, bool bVertexColors, bool bReverseOrientation)
{	

	SpawnedActor = GetWorld()->SpawnActor<AActor>();
	


	const char* fileToImport = "C:/Users/Ken/Desktop/701demo.fbx";
	const bool initStatus = importer->Initialize(fileToImport);

	int lFileMajor, lFileMinor, lFileRevision;
	importer->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_MATERIAL, true);
	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_TEXTURE, true);
	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_LINK, false);
	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_SHAPE, false);
	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_GOBO, false);
	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_ANIMATION, false);
	(*(SdkManager->GetIOSettings())).SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);

	FbxScene* Scene = FbxScene::Create(SdkManager, "");

	const bool importStatus = importer->Import(Scene);

	// Get Root Node
	FbxNode* rootNode = Scene->GetRootNode();

	IterateNode(rootNode);




	return true;
}


UE_ENABLE_OPTIMIZATION