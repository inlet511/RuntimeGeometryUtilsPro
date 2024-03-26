#include "FBXReader.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

THIRD_PARTY_INCLUDES_START
#include <fbxsdk.h>
THIRD_PARTY_INCLUDES_END


UE_DISABLE_OPTIMIZATION

using namespace UE::Geometry;




void IterateNode(FbxNode* node)
{
	int count = node->GetChildCount();
	for (int i = 0; i < count; i++)
	{
		FbxNodeAttribute* attribute = node->GetChild(i)->GetNodeAttribute();
		if (attribute != nullptr)
		{
			FbxNodeAttribute::EType type = attribute->GetAttributeType();
			const char* nodeName = node->GetChild(i)->GetName();
			FString str = UTF8_TO_TCHAR(nodeName);

			switch (type)
			{
			case FbxNodeAttribute::eMesh:
				UE_LOG(LogTemp, Warning, TEXT("[Mesh] %s"), *str);
				break;

			case FbxNodeAttribute::eNull:
				UE_LOG(LogTemp, Warning, TEXT("[Null] %s"), *str);
				break;

			case FbxNodeAttribute::eUnknown:
				UE_LOG(LogTemp, Warning, TEXT("[Unknown] %s"), *str);

			default:
				UE_LOG(LogTemp, Warning, TEXT("[Other] %s"), *str);
				break;
			}

			IterateNode(node->GetChild(i));
		}
	}
}

bool RTGUtils::ReadFBXMesh(
	const FString& Path,
	FDynamicMesh3& MeshOut,
	bool bNormals,
	bool bTexCoords,
	bool bVertexColors,
	bool bReverseOrientation)
{
	FbxManager* SdkManager = FbxManager::Create();

	FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
	SdkManager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(SdkManager, "");

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


	SdkManager->Destroy();
	SdkManager = nullptr;

	return true;
}

UE_ENABLE_OPTIMIZATION