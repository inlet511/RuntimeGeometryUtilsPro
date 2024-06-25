﻿#include "DynamicMeshBaseActor.h"

#include "Generators/SphereGenerator.h"
#include "Generators/GridBoxMeshGenerator.h"
#include "MeshQueries.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "MeshSimplification.h"
#include "Operations/MeshBoolean.h"
#include "Implicit/Solidify.h"
#include "Implicit/Morphology.h"


#include "DynamicMeshOBJReader.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "StaticMeshAttributes.h"

#include "Modules/ModuleManager.h"
#include "IMeshReductionManagerModule.h"
#include "IMeshReductionInterfaces.h"

#include "ModelingToolTargetUtil.h"

#include "CleaningOps/HoleFillOp.h"
#include "MeshBoundaryLoops.h"

#include "DynamicMeshOBJWriter.h"
#include "DynamicFBXImporter.h"
#include "..\Public\Generators\ConvexHullGenerator.h"

using namespace UE::Geometry;

// Sets default values
ADynamicMeshBaseActor::ADynamicMeshBaseActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AccumulatedTime = 0;
	MeshAABBTree.SetMesh(&SourceMesh);

	FastWinding = MakeUnique<TFastWindingTree<FDynamicMesh3>>(&MeshAABBTree, false);
}



void ADynamicMeshBaseActor::PostLoad()
{
	Super::PostLoad();
	OnMeshGenerationSettingsModified();
}

void ADynamicMeshBaseActor::PostActorCreated()
{
	Super::PostActorCreated();
	OnMeshGenerationSettingsModified();
}



// Called when the game starts or when spawned
void ADynamicMeshBaseActor::BeginPlay()
{
	Super::BeginPlay();

	AccumulatedTime = 0;
	OnMeshGenerationSettingsModified();
}

// Called every frame
void ADynamicMeshBaseActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AccumulatedTime += DeltaTime;
	if (bRegenerateOnTick && SourceType == EDynamicMeshActorSourceType::Primitive)
	{
		OnMeshGenerationSettingsModified();
	}
}


#if WITH_EDITOR
void ADynamicMeshBaseActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnMeshGenerationSettingsModified();
}
#endif


void ADynamicMeshBaseActor::EditMesh(TFunctionRef<void(FDynamicMesh3&)> EditFunc)
{
	EditFunc(SourceMesh);

	// update spatial data structures
	if (bEnableSpatialQueries || bEnableInsideQueries)
	{
		MeshAABBTree.Build();
		if (bEnableInsideQueries)
		{
			FastWinding->Build();
		}
	}

	OnMeshEditedInternal();
}


void ADynamicMeshBaseActor::GetMeshCopy(FDynamicMesh3& MeshOut)
{
	MeshOut = SourceMesh;
}

const FDynamicMesh3& ADynamicMeshBaseActor::GetMeshRef() const
{
	return SourceMesh;
}

void ADynamicMeshBaseActor::OnMeshEditedInternal()
{
	OnMeshModified.Broadcast(this);
}


void ADynamicMeshBaseActor::OnMeshGenerationSettingsModified()
{
	EditMesh([this](FDynamicMesh3& MeshToUpdate) {
		RegenerateSourceMesh(MeshToUpdate);
		});
}


void ADynamicMeshBaseActor::RegenerateSourceMesh(FDynamicMesh3& MeshOut)
{
	if (SourceType == EDynamicMeshActorSourceType::Primitive)
	{
		double UseRadius = (this->MinimumRadius + this->VariableRadius)
			+ (this->VariableRadius) * FMathd::Sin(PulseSpeed * AccumulatedTime);

		// generate new mesh
		if (this->PrimitiveType == EDynamicMeshActorPrimitiveType::Sphere)
		{
			FSphereGenerator SphereGen;
			SphereGen.NumPhi = SphereGen.NumTheta = FMath::Clamp(this->TessellationLevel, 3, 50);
			SphereGen.Radius = UseRadius;
			MeshOut.Copy(&SphereGen.Generate());
		}
		else if(this->PrimitiveType == EDynamicMeshActorPrimitiveType::Box)
		{
			FGridBoxMeshGenerator BoxGen;
			int TessLevel = FMath::Clamp(this->TessellationLevel, 2, 50);
			BoxGen.EdgeVertices = FIndex3i(TessLevel, TessLevel, TessLevel);
			FVector3d BoxExtents = UseRadius * FVector3d::One();
			BoxExtents.Z *= BoxDepthRatio;
			BoxGen.Box = FOrientedBox3d(FVector3d::Zero(), BoxExtents);
			MeshOut.Copy(&BoxGen.Generate());
		}else if(this->PrimitiveType == EDynamicMeshActorPrimitiveType::TriangulateConvexHull)
		{
			if(RandomPoints.Num()<3)
				return;
			FConvexHullGenerator ConvexHullGenerator;
			ConvexHullGenerator.InputVertices = RandomPoints;
			MeshOut.Copy(&ConvexHullGenerator.Generate());
		}
		

	}
	else if (SourceType == EDynamicMeshActorSourceType::ImportedMesh)
	{
		FString UsePath = ImportPath;
		if (FPaths::FileExists(UsePath) == false && FPaths::IsRelative(UsePath))
		{
			UsePath = FPaths::ProjectContentDir() + ImportPath;
		}

		MeshOut = FDynamicMesh3();
		if (!RTGUtils::ReadOBJMesh(UsePath, MeshOut, true, true, true, bReverseOrientation))
		{
			UE_LOG(LogTemp, Warning, TEXT("Error reading mesh file %s"), *UsePath);
			FSphereGenerator SphereGen;
			SphereGen.NumPhi = SphereGen.NumTheta = 8;
			SphereGen.Radius = this->MinimumRadius;
			MeshOut.Copy(&SphereGen.Generate());
		}
		//if (!RTGUtils::ReadFBXMesh(UsePath, MeshOut, true, true, true, bReverseOrientation))
		//{

		//}

		if (bCenterPivot)
		{
			MeshTransforms::Translate(MeshOut, -MeshOut.GetBounds().Center());
		}

		if (ImportScale != 1.0)
		{
			MeshTransforms::Scale(MeshOut, ImportScale * FVector3d::One(), FVector3d::Zero());
		}
	}
	else if (SourceType == EDynamicMeshActorSourceType::FromStaticMesh)
	{
		MeshOut = FDynamicMesh3();
		if (RefStaticMesh != nullptr)
		{
			FMeshDescription* MeshDescription = RefStaticMesh->GetMeshDescription(0);
			FMeshDescriptionToDynamicMesh Converter;
			Converter.Convert(MeshDescription, MeshOut, true);
		}
	}

	RecomputeNormals(MeshOut);
}



void ADynamicMeshBaseActor::RecomputeNormals(FDynamicMesh3& MeshOut)
{
	if (this->NormalsMode == EDynamicMeshActorNormalsMode::PerVertexNormals)
	{
		MeshOut.EnableAttributes();
		FMeshNormals::InitializeOverlayToPerVertexNormals(MeshOut.Attributes()->PrimaryNormals(), false);
	}
	else if (this->NormalsMode == EDynamicMeshActorNormalsMode::FaceNormals)
	{
		MeshOut.EnableAttributes();
		FMeshNormals::InitializeOverlayToPerTriangleNormals(MeshOut.Attributes()->PrimaryNormals());
	}
}



int ADynamicMeshBaseActor::GetTriangleCount()
{
	return SourceMesh.TriangleCount();
}


float ADynamicMeshBaseActor::DistanceToPoint(FVector WorldPoint, FVector& NearestWorldPoint, int& NearestTriangle, FVector& TriBaryCoords)
{
	NearestWorldPoint = WorldPoint;
	NearestTriangle = -1;
	if (bEnableSpatialQueries == false)
	{
		return TNumericLimits<float>::Max();
	}

	FTransform3d ActorToWorld(GetActorTransform());
	FVector3d LocalPoint = ActorToWorld.InverseTransformPosition((FVector3d)WorldPoint);

	double NearDistSqr;
	NearestTriangle = MeshAABBTree.FindNearestTriangle(LocalPoint, NearDistSqr);
	if (NearestTriangle < 0)
	{
		return TNumericLimits<float>::Max();
	}

	FDistPoint3Triangle3d DistQuery = TMeshQueries<FDynamicMesh3>::TriangleDistance(SourceMesh, NearestTriangle, LocalPoint);
	NearestWorldPoint = (FVector)ActorToWorld.TransformPosition(DistQuery.ClosestTrianglePoint);
	TriBaryCoords = (FVector)DistQuery.TriangleBaryCoords;
	return (float)FMathd::Sqrt(NearDistSqr);
}


FVector ADynamicMeshBaseActor::NearestPoint(FVector WorldPoint)
{
	if (bEnableSpatialQueries)
	{
		FTransform3d ActorToWorld(GetActorTransform());
		FVector3d LocalPoint = ActorToWorld.InverseTransformPosition((FVector3d)WorldPoint);
		return (FVector)ActorToWorld.TransformPosition(MeshAABBTree.FindNearestPoint(LocalPoint));
	}
	return WorldPoint;
}

bool ADynamicMeshBaseActor::ContainsPoint(FVector WorldPoint, float WindingThreshold)
{
	if (bEnableInsideQueries)
	{
		FTransform3d ActorToWorld(GetActorTransform());
		FVector3d LocalPoint = ActorToWorld.InverseTransformPosition((FVector3d)WorldPoint);
		return FastWinding->IsInside(LocalPoint, WindingThreshold);
	}
	return false;
}


bool ADynamicMeshBaseActor::IntersectRay(FVector RayOrigin, FVector RayDirection,
	FVector& WorldHitPoint, float& HitDistance, int& NearestTriangle, FVector& TriBaryCoords,
	float MaxDistance)
{
	if (bEnableSpatialQueries)
	{
		FTransform3d ActorToWorld(GetActorTransform());
		FMatrix Matrix = ActorToWorld.ToMatrixWithScale();
		FMatrix MatrixTA = Matrix.TransposeAdjoint();
		FMatrix::FReal DetM = Matrix.Determinant();
		FVector3d WorldDirection(RayDirection); WorldDirection.Normalize();
		FRay3d LocalRay(ActorToWorld.InverseTransformPosition((FVector3d)RayOrigin),
			ActorToWorld.InverseTransformVectorNoScale(WorldDirection));
		IMeshSpatial::FQueryOptions QueryOptions;
		if (MaxDistance > 0)
		{
			QueryOptions.MaxDistance = MaxDistance;
		}
		NearestTriangle = MeshAABBTree.FindNearestHitTriangle(LocalRay, QueryOptions);
		if (SourceMesh.IsTriangle(NearestTriangle))
		{
			FIntrRay3Triangle3d IntrQuery = TMeshQueries<FDynamicMesh3>::TriangleIntersection(SourceMesh, NearestTriangle, LocalRay);
			if (IntrQuery.IntersectionType == EIntersectionType::Point)
			{
				HitDistance = IntrQuery.RayParameter;
				WorldHitPoint = (FVector)ActorToWorld.TransformPosition(LocalRay.PointAt(IntrQuery.RayParameter));
				TriBaryCoords = (FVector)IntrQuery.TriangleBaryCoords;
				return true;
			}
		}
	}
	return false;
}




void ADynamicMeshBaseActor::SubtractMesh(ADynamicMeshBaseActor* OtherMeshActor)
{
	BooleanWithMesh(OtherMeshActor, EDynamicMeshActorBooleanOperation::Subtraction);
}
void ADynamicMeshBaseActor::UnionWithMesh(ADynamicMeshBaseActor* OtherMeshActor)
{
	BooleanWithMesh(OtherMeshActor, EDynamicMeshActorBooleanOperation::Union);
}
void ADynamicMeshBaseActor::IntersectWithMesh(ADynamicMeshBaseActor* OtherMeshActor)
{
	BooleanWithMesh(OtherMeshActor, EDynamicMeshActorBooleanOperation::Intersection);
}


void ADynamicMeshBaseActor::BooleanWithMesh(ADynamicMeshBaseActor* OtherMeshActor, EDynamicMeshActorBooleanOperation Operation)
{
	if (ensure(OtherMeshActor) == false) return;

	FTransform3d ActorToWorld(GetActorTransform());
	FTransform3d OtherToWorld(OtherMeshActor->GetActorTransform());

	FDynamicMesh3 OtherMesh;
	OtherMeshActor->GetMeshCopy(OtherMesh);
	MeshTransforms::ApplyTransform(OtherMesh, OtherToWorld);
	MeshTransforms::ApplyTransformInverse(OtherMesh, ActorToWorld);

	EditMesh([&](FDynamicMesh3& MeshToUpdate) {

		FDynamicMesh3 ResultMesh;

		FMeshBoolean::EBooleanOp ApplyOp = FMeshBoolean::EBooleanOp::Union;
		switch (Operation)
		{
		default:
			break;
		case EDynamicMeshActorBooleanOperation::Subtraction:
			ApplyOp = FMeshBoolean::EBooleanOp::Difference;
			break;
		case EDynamicMeshActorBooleanOperation::Intersection:
			ApplyOp = FMeshBoolean::EBooleanOp::Intersect;
			break;
		}

		FMeshBoolean Boolean(
			&MeshToUpdate, FTransform3d::Identity,
			&OtherMesh, FTransform3d::Identity,
			&ResultMesh,
			ApplyOp);
		Boolean.bPutResultInInputSpace = true;
		bool bOK = Boolean.Compute();

		if (!bOK)
		{
			// fill holes
		}

		RecomputeNormals(ResultMesh);

		MeshToUpdate = MoveTemp(ResultMesh);
		});
}



bool ADynamicMeshBaseActor::ImportMesh(FString Path, bool bFlipOrientation, bool bRecomputeNormals)
{
	FDynamicMesh3 ImportedMesh;
	if (!RTGUtils::ReadOBJMesh(Path, ImportedMesh, true, true, true, bFlipOrientation))
	{
		UE_LOG(LogTemp, Warning, TEXT("Error reading mesh file %s"), *Path);
		return false;
	}

	if (bRecomputeNormals)
	{
		RecomputeNormals(ImportedMesh);
	}

	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate = MoveTemp(ImportedMesh);
		});

	return true;
}


void ADynamicMeshBaseActor::CopyFromMesh(ADynamicMeshBaseActor* OtherMesh, bool bRecomputeNormals)
{
	if (!ensure(OtherMesh)) return;

	// the part where we generate a new mesh
	FDynamicMesh3 TmpMesh;
	OtherMesh->GetMeshCopy(TmpMesh);

	// apply our normals setting
	if (bRecomputeNormals)
	{
		RecomputeNormals(TmpMesh);
	}

	// update the mesh
	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate = MoveTemp(TmpMesh);
		});
}


void ADynamicMeshBaseActor::SolidifyMesh(int VoxelResolution, float WindingThreshold)
{
	if (MeshAABBTree.IsValid(true) == false)
	{
		MeshAABBTree.Build();
	}
	if (FastWinding->IsBuilt() == false)
	{
		FastWinding->Build();
	}

	// ugh workaround for bug
	FDynamicMesh3 CompactMesh;
	CompactMesh.CompactCopy(SourceMesh, false, false, false, false);
	FDynamicMeshAABBTree3 AABBTree(&CompactMesh, true);
	TFastWindingTree<FDynamicMesh3> Winding(&AABBTree, true);

	double ExtendBounds = 2.0;
	//TImplicitSolidify<FDynamicMesh3> SolidifyCalc(&SourceMesh, &MeshAABBTree, FastWinding.Get());
	//SolidifyCalc.SetCellSizeAndExtendBounds(MeshAABBTree.GetBoundingBox(), ExtendBounds, VoxelResolution);
	TImplicitSolidify<FDynamicMesh3> SolidifyCalc(&CompactMesh, &AABBTree, &Winding);
	SolidifyCalc.SetCellSizeAndExtendBounds(AABBTree.GetBoundingBox(), ExtendBounds, VoxelResolution);
	SolidifyCalc.WindingThreshold = WindingThreshold;
	SolidifyCalc.SurfaceSearchSteps = 5;
	SolidifyCalc.bSolidAtBoundaries = true;
	SolidifyCalc.ExtendBounds = ExtendBounds;
	FDynamicMesh3 SolidMesh(&SolidifyCalc.Generate());

	SolidMesh.EnableAttributes();
	RecomputeNormals(SolidMesh);

	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate = MoveTemp(SolidMesh);
		});
}


void ADynamicMeshBaseActor::SimplifyMeshToTriCount(int32 TargetTriangleCount)
{
	TargetTriangleCount = FMath::Max(1, TargetTriangleCount);
	if (TargetTriangleCount >= SourceMesh.TriangleCount()) return;

	// make compacted copy because it seems to change the results?
	FDynamicMesh3 SimplifyMesh;
	SimplifyMesh.CompactCopy(SourceMesh, false, false, false, false);
	SimplifyMesh.EnableTriangleGroups();			// workaround for failing check()
	FQEMSimplification Simplifier(&SimplifyMesh);
	Simplifier.SimplifyToTriangleCount(TargetTriangleCount);
	SimplifyMesh.EnableAttributes();
	RecomputeNormals(SimplifyMesh);

	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate.CompactCopy(SimplifyMesh);
		});
}


void ADynamicMeshBaseActor::SimplifyMesh(ESimplifyTargetType simplifyTargetType, int32 percent, int32 targetTriangleCount)
{
	if (MeshAABBTree.IsValid(true) == false)
	{
		MeshAABBTree.Build();
	}
	if (FastWinding->IsBuilt() == false)
	{
		FastWinding->Build();
	}

	// Prepare DynamicMesh and AABBTree
	TSharedPtr<FDynamicMesh3> MeshCopy = MakeShared<FDynamicMesh3>();
	MeshCopy->Copy(SourceMesh);
	TSharedPtr<FDynamicMeshAABBTree3, ESPMode::ThreadSafe>  AABBTreePtr = MakeShared<FDynamicMeshAABBTree3, ESPMode::ThreadSafe>(MeshCopy.Get(), true);


	MeshCopy->EnableTriangleGroups();
	MeshCopy->EnableAttributes();

	// Prepare MeshDescription
	TSharedPtr<FMeshDescription, ESPMode::ThreadSafe> OriginalMeshDescription = MakeShared<FMeshDescription, ESPMode::ThreadSafe>();
	FStaticMeshAttributes Attributes(*OriginalMeshDescription);
	Attributes.Register();
	FDynamicMeshToMeshDescription Converter;
	Converter.Convert(MeshCopy.Get(), *OriginalMeshDescription);


	// Simplifier Setup
	FSimplifyMeshOp Simplifier;
	Simplifier.OriginalMesh = MeshCopy;
	Simplifier.OriginalMeshSpatial = AABBTreePtr;

	Simplifier.OriginalMeshDescription = OriginalMeshDescription;
	Simplifier.TargetMode = simplifyTargetType;
	Simplifier.TargetPercentage = percent;
	Simplifier.TargetCount = targetTriangleCount;

	// 如果这个选项要选择UEStandard, 则需要MeshDescription,以及MeshReduction
	// MeshDescription 已经正确运行
	Simplifier.SimplifierType = ESimplifyType::Attribute;
	Simplifier.SetTransform(this->GetTransform());


	IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	Simplifier.MeshReduction = MeshReductionModule.GetStaticMeshReductionInterface();

	Simplifier.CalculateResult(nullptr);

	TUniquePtr<FDynamicMesh3> NewResultMesh = Simplifier.ExtractResult();
	FDynamicMesh3* NewResultMeshPtr = NewResultMesh.Release();

	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate.CompactCopy(*NewResultMeshPtr);
		});
}


void ADynamicMeshBaseActor::DilateMesh(float distance, float gridCellSize, float meshCellSize)
{
	TImplicitMorphology<FDynamicMesh3> Mopher;
	FDynamicMesh3 SimplifyMesh;
	SimplifyMesh.CompactCopy(SourceMesh, false, false, false, false);
	FDynamicMeshAABBTree3 AABBTree(&SimplifyMesh, true);

	Mopher.Source = &SimplifyMesh;
	Mopher.SourceSpatial = &AABBTree;
	Mopher.MorphologyOp = TImplicitMorphology<FDynamicMesh3>::EMorphologyOp::Dilate;
	Mopher.Distance = distance;
	Mopher.GridCellSize = gridCellSize;
	Mopher.MeshCellSize = meshCellSize;

	FDynamicMesh3 NewMesh(&Mopher.Generate());
	NewMesh.EnableAttributes();
	RecomputeNormals(NewMesh);

	EditMesh([&](FDynamicMesh3& MeshToUpdate) {

		MeshToUpdate = MoveTemp(NewMesh);
		});
}

void ADynamicMeshBaseActor::FillHole(int32& NumFilledHoles, int32& NumFailedHoleFills)
{
	if (MeshAABBTree.IsValid(true) == false)
	{
		MeshAABBTree.Build();
	}
	if (FastWinding->IsBuilt() == false)
	{
		FastWinding->Build();
	}

	TSharedPtr<FDynamicMesh3> MeshCopy = MakeShared<FDynamicMesh3>();
	MeshCopy->Copy(SourceMesh);

	FMeshBoundaryLoops Loops(MeshCopy.Get(), true);


	FHoleFillOp HoleFiller;
	HoleFiller.OriginalMesh = MeshCopy;
	HoleFiller.Loops = MoveTemp(Loops.Loops);

	HoleFiller.FillType = EHoleFillOpFillType::Minimal;
	HoleFiller.FillOptions.bRemoveIsolatedTriangles = true;
	HoleFiller.FillOptions.bQuickFillSmallHoles = true;

	HoleFiller.CalculateResult(nullptr);

	NumFailedHoleFills = HoleFiller.NumFailedLoops;
	NumFilledHoles = HoleFiller.Loops.Num() - NumFailedHoleFills;

	TUniquePtr<FDynamicMesh3> NewResultMesh = HoleFiller.ExtractResult();
	FDynamicMesh3* NewResultMeshPtr = NewResultMesh.Release();

	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate.CompactCopy(*NewResultMeshPtr);
		});
}


void ADynamicMeshBaseActor::WriteObj(const FString OutputPath)
{
	RTGUtils::WriteOBJMesh(OutputPath, SourceMesh, true);
}
