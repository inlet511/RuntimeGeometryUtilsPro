#include "DynamicFBXImporter.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Components/DynamicMeshComponent.h"
#include "ImportedFBXActor.h"


UE_DISABLE_OPTIMIZATION

using namespace UE::Geometry;


ADynamicFBXImporter::ADynamicFBXImporter()
{

}

ADynamicFBXImporter::~ADynamicFBXImporter()
{
}

void ADynamicFBXImporter::BuildMesh(FbxNode* node, UE::Geometry::FDynamicMesh3& MeshOut)
{
	FbxMesh* fbx_mesh = node->GetMesh();
	fbxsdk::FbxVector4* vertices = fbx_mesh->GetControlPoints();
	int numVertices = fbx_mesh->GetControlPointsCount();

	int numIndices = fbx_mesh->GetPolygonVertexCount();
	int* indices = fbx_mesh->GetPolygonVertices();

	fbxsdk::FbxLayerElementUV* pUVs = fbx_mesh->GetLayer(0)->GetUVs();
	fbxsdk::FbxLayerElementArrayTemplate<fbxsdk::FbxVector2>* pUVArray = &pUVs->GetDirectArray();

	fbxsdk::FbxGeometryElementNormal* pNormals = fbx_mesh->GetElementNormal();
	fbxsdk::FbxLayerElementArrayTemplate<fbxsdk::FbxVector4>* pNormalArray = &pNormals->GetDirectArray();

	// Enable Attributes(Normals, UVs)
	MeshOut.EnableAttributes();

	FDynamicMeshNormalOverlay* Normals = MeshOut.Attributes()->PrimaryNormals();
	FDynamicMeshUVOverlay* UVs = MeshOut.Attributes()->PrimaryUV();

	// Iterate Vertices
	for (size_t i = 0; i < numVertices; i++)
	{
		// Append Vertices
		MeshOut.AppendVertex(FVector3d(vertices[i][0], vertices[i][1], vertices[i][2]));

		// Append Normals
		fbxsdk::FbxVector4 normal = pNormalArray->GetAt(i);
		Normals->AppendElement(FVector3f(normal[0], normal[1], normal[2]));

		// Append UVs
		fbxsdk::FbxVector2 uv = pUVArray->GetAt(i);
		UVs->AppendElement(FVector2f(uv[0], uv[1]));
	}

	// Append Indices
	for (size_t i = 0; i < numIndices/3; i++)
	{
		MeshOut.AppendTriangle(*(indices + (3 * i)), *(indices + (3 * i) + 1), *(indices + (3 * i) + 2));
	}

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
		{
			UE_LOG(LogTemp, Warning, TEXT("[Mesh] %s"), *str);

			FString ComponentName = FString::Printf(TEXT("%s"), *str);

			FDynamicMesh3 DynamicMesh;
			BuildMesh(node, DynamicMesh);
			SpawnedActor->AddMeshComponent(FName(*ComponentName),DynamicMesh);

			break;
		}

		case FbxNodeAttribute::eNull:
		{
			UE_LOG(LogTemp, Warning, TEXT("[Null] %s"), *str);
			break;
		}

		case FbxNodeAttribute::eUnknown:
		{
			UE_LOG(LogTemp, Warning, TEXT("[Unknown] %s"), *str);
			break;
		}

		default:
		{
			UE_LOG(LogTemp, Warning, TEXT("[Other] %s"), *str);
			break;
		}
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
	// Create SDK Manager
	SdkManager = FbxManager::Create();

	// Create IO Settings
	ios = FbxIOSettings::Create(SdkManager, IOSROOT);

	ios->SetBoolProp(IMP_FBX_MATERIAL, true);
	ios->SetBoolProp(IMP_FBX_MATERIAL, true);
	ios->SetBoolProp(IMP_FBX_TEXTURE, true);
	ios->SetBoolProp(IMP_FBX_LINK, false);
	ios->SetBoolProp(IMP_FBX_SHAPE, false);
	ios->SetBoolProp(IMP_FBX_GOBO, false);
	ios->SetBoolProp(IMP_FBX_ANIMATION, false);
	ios->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);

	SdkManager->SetIOSettings(ios);

	// Create Importer
	importer = FbxImporter::Create(SdkManager, "");

	// Spawn an Actor
	SpawnedActor = GetWorld()->SpawnActor<AImportedFBXActor>();


	const char* fileToImport = "C:/Users/Ken/Desktop/701demo.fbx";

	// Initialize importer
	const bool initStatus = importer->Initialize(fileToImport,-1,SdkManager->GetIOSettings());
	
	// Create a scene
	FbxScene* Scene = FbxScene::Create(SdkManager, "Scene");

	// Import into the scene
	const bool importStatus = importer->Import(Scene);

	// Triangulate Mesh
	FbxGeometryConverter converter(SdkManager);
	converter.Triangulate(Scene, true);

	// Get Root Node
	FbxNode* rootNode = Scene->GetRootNode();

	// Iterate node
	IterateNode(rootNode);



	// Destroy 
	importer->Destroy();
	importer = nullptr;
	Scene->Destroy();
	Scene = nullptr;
	SdkManager->Destroy();
	SdkManager = nullptr;

	return true;
}



UE_ENABLE_OPTIMIZATION