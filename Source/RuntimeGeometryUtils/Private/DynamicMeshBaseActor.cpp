#include "DynamicMeshBaseActor.h"

#include "ConstrainedDelaunay2.h"
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
#include "MeshOpPreviewHelpers.h"
#include "../Public/Generators/ConvexHullGenerator.h"
#include "../Public/Generators/RandomPointsMeshGenerator.h"
#include "../Public/Generators/FDelaunayGenrator.h"
#include "CuttingOps/PlaneCutOp.h"
#include "Operations/MeshPlaneCut.h"
#include "DynamicMeshEditor.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "Misc/FileHelper.h"
#include "MeshComponentRuntimeUtils.h"
#include "Probe.h"


using namespace UE::Geometry;

UE_DISABLE_OPTIMIZATION

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

void ADynamicMeshBaseActor::ForceRegenerate()
{
	OnMeshGenerationSettingsModified();
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
	if (SourceType == EDynamicMeshActorSourceType::None)
	{
		// Do Nothing.
	}
	else if (SourceType == EDynamicMeshActorSourceType::Primitive)
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
		else if (this->PrimitiveType == EDynamicMeshActorPrimitiveType::Box)
		{
			FGridBoxMeshGenerator BoxGen;
			int TessLevel = FMath::Clamp(this->TessellationLevel, 2, 50);
			BoxGen.EdgeVertices = FIndex3i(TessLevel, TessLevel, TessLevel);
			FVector3d BoxExtents = UseRadius * FVector3d::One();
			BoxExtents.Z *= BoxDepthRatio;
			BoxGen.Box = FOrientedBox3d(FVector3d::Zero(), BoxExtents);
			MeshOut.Copy(&BoxGen.Generate());
		}
		else if (this->PrimitiveType == EDynamicMeshActorPrimitiveType::ConvexHull)
		{
			if (RandomPoints.Num() < 3)
				return;
			FConvexHullGenerator ConvexHullGenerator;
			ConvexHullGenerator.InputVertices = RandomPoints;
			MeshOut.Copy(&ConvexHullGenerator.Generate());
		}
		else if (this->PrimitiveType == EDynamicMeshActorPrimitiveType::RandomPoints)
		{
			if (RandomPoints.Num() < 3)
				return;
			FRandomPointsMeshGenerator RandomPointsMeshGenerator;
			RandomPointsMeshGenerator.InputVertices = RandomPoints;
			MeshOut.Copy(&RandomPointsMeshGenerator.Generate());
		}
		else if (this->PrimitiveType == EDynamicMeshActorPrimitiveType::Delaunay)
		{
			if (RandomPoints.Num() < 3)
				return;
			FDelaunayGenrator DelaunayGenrator;
			DelaunayGenrator.InputVertices = RandomPoints;
			MeshOut.Copy(&DelaunayGenrator.Generate());
		}
		else if (this->PrimitiveType == EDynamicMeshActorPrimitiveType::MarchingCubes)
		{
			if (SpatialPoints.Num() < 3)
				return;

			TArray<FVector> Positions;
			for (auto Point : SpatialPoints)
			{
				Positions.Add(Point->GetActorLocation());
			}
			FMarchingCubes MarchingCubes;
			MarchingCubes.CubeSize = Cubesize;
			// find the bounds
			TAxisAlignedBox3<double> Bounds;
			RTGUtils::FindAABounds(Bounds, Positions);

			MarchingCubes.Bounds = Bounds;
			MarchingCubes.Bounds.Expand(200);
			MarchingCubes.IsoValue = 0.5f;
			MarchingCubes.RootMode = ERootfindingModes::Bisection;
			MarchingCubes.RootModeSteps = 4;
			MarchingCubes.bParallelCompute = true;
			MarchingCubes.Implicit = [this](const FVector3d& Pos)->double
				{
					double MinDist = BIG_NUMBER;
					for (auto P : SpatialPoints)
					{
						double distance = FVector3d::Dist(Pos, P->GetActorLocation());
						
						
						if(distance < MinDist)
						{
							MinDist = distance;
						}
					}
					return 100/MinDist;
				};

			MeshOut.Copy(&MarchingCubes.Generate());
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

void ADynamicMeshBaseActor::PlaneCut(ADynamicMeshBaseActor* OtherMeshActor, FVector PlaneOrigin, FVector PlaneNormal, float GapWidth, bool bFillCutHole, bool bFillSpans, bool bKeepBothHalves)
{
	auto Start = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	// 给三角形添加自定义属性
	const FName ObjectIndexAttribute = "ObjectIndexAttribute";
	TDynamicMeshScalarTriangleAttribute<int>* SubObjectAttrib = new TDynamicMeshScalarTriangleAttribute<int>(&SourceMesh);
	SubObjectAttrib->Initialize(0);
	SourceMesh.Attributes()->AttachAttribute(ObjectIndexAttribute, SubObjectAttrib);

	// 拷贝原始Mesh
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> SourceMeshPtr = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();
	SourceMeshPtr->Copy(SourceMesh);
	SourceMeshPtr->EnableAttributes();

	// 从世界坐标转换到局部坐标
	FTransform LocalToWorld = GetTransform();
	FTransform WorldToLocal = LocalToWorld.Inverse();
	FVector LocalOrigin = WorldToLocal.TransformPosition(PlaneOrigin);
	FVector LocalNormal = WorldToLocal.TransformVector(PlaneNormal);

	// 创建PlaneCut
	FPlaneCutOp PlaneCut;
	PlaneCut.SetTransform(LocalToWorld);
	PlaneCut.LocalPlaneOrigin = LocalOrigin;
	PlaneCut.LocalPlaneNormal = LocalNormal;
	PlaneCut.bFillCutHole = bFillCutHole;
	PlaneCut.bFillSpans = bFillSpans;
	PlaneCut.bKeepBothHalves = bKeepBothHalves;
	PlaneCut.OriginalMesh = SourceMeshPtr;
	FProgressCancel ProgressCancel;
	PlaneCut.CalculateResult(&ProgressCancel);
	TUniquePtr<FDynamicMesh3> ResultMesh = PlaneCut.ExtractResult();

	TDynamicMeshScalarTriangleAttribute<int>* SubMeshIDs = static_cast<TDynamicMeshScalarTriangleAttribute<int>*>(ResultMesh->Attributes()->GetAttachedAttribute(ObjectIndexAttribute));

	// 根据三角形的Attribute划分为两个SourceMesh
	TArray<FDynamicMesh3> SplitMeshes;
	bool bSucceeded = FDynamicMeshEditor::SplitMesh(ResultMesh.Get(), SplitMeshes, [SubMeshIDs](int TID)
		{
			return SubMeshIDs->GetValue(TID);
		});

	if (!bSucceeded)
		return;

	SplitMeshes[0].Attributes()->RemoveAttribute(ObjectIndexAttribute);
	SplitMeshes[1].Attributes()->RemoveAttribute(ObjectIndexAttribute);

	// 更新 "我的" Mesh
	NormalsMode = EDynamicMeshActorNormalsMode::SplitNormals;
	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate = MoveTemp(SplitMeshes[0]);
			RecomputeNormals(MeshToUpdate);
		});


	// 更新 "新的" Mesh
	if (IsValid(OtherMeshActor) && SplitMeshes.Num() == 2)
	{
		OtherMeshActor->NormalsMode = EDynamicMeshActorNormalsMode::SplitNormals;
		OtherMeshActor->SetActorLocation(this->GetActorLocation());
		OtherMeshActor->SetActorRotation(this->GetActorRotation());
		OtherMeshActor->EditMesh([&](FDynamicMesh3& MeshToUpdate)
			{
				MeshToUpdate = MoveTemp(SplitMeshes[1]);
				RecomputeNormals(MeshToUpdate);
			});
	}

	auto End = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();

	UE_LOG(LogTemp, Warning, TEXT("Plane Cut cost : %f ms"), End - Start);
}

void ADynamicMeshBaseActor::SetIsShell(FDynamicMesh3& InSourceMesh, const FMeshPlaneCut& Cut)
{
	// 给三角形添加"IsShell"属性, 表示最初的外层壳
	const FName IsShellName = "bIsShell";
	TDynamicMeshScalarTriangleAttribute<bool>* IsShellAtt = nullptr;

	if (InSourceMesh.Attributes())
	{
		// 模型包含 IsShell 属性, 直接添加
		if (InSourceMesh.Attributes()->HasAttachedAttribute(IsShellName))
		{
			IsShellAtt = static_cast<TDynamicMeshScalarTriangleAttribute<bool>*>(InSourceMesh.Attributes()->GetAttachedAttribute(IsShellName));
		}
		else // 模型不包含 IsShell 属性, 需要添加
		{
			IsShellAtt = new TDynamicMeshScalarTriangleAttribute<bool>(&InSourceMesh);
			IsShellAtt->Initialize(true); //初始化时都为true
			InSourceMesh.Attributes()->AttachAttribute(IsShellName, IsShellAtt);
		}
	}

	// 切割面上的三角形索引
	TArray<TArray<int32>> HoleFillTriangles = Cut.HoleFillTriangles;

	// 设置 IsShell为 false
	for (int BoundaryIdx = 0; BoundaryIdx < Cut.OpenBoundaries.Num(); BoundaryIdx++)
	{
		const TArray<int>& Triangles = HoleFillTriangles[BoundaryIdx];
		const FMeshPlaneCut::FOpenBoundary& Boundary = Cut.OpenBoundaries[BoundaryIdx];
		for (const int Tid : Triangles)
		{
			IsShellAtt->SetValue(Tid, false);
		}
	}
}

namespace SplitMeshInternal
{
	static int AppendTriangleUVAttribute(const FDynamicMesh3* FromMesh, int FromElementID, FDynamicMesh3* ToMesh, int UVLayerIndex, FMeshIndexMappings& IndexMaps)
	{
		int NewElementID = IndexMaps.GetNewUV(UVLayerIndex, FromElementID);
		if (NewElementID == IndexMaps.InvalidID())
		{
			const FDynamicMeshUVOverlay* FromUVOverlay = FromMesh->Attributes()->GetUVLayer(UVLayerIndex);
			FDynamicMeshUVOverlay* ToUVOverlay = ToMesh->Attributes()->GetUVLayer(UVLayerIndex);
			NewElementID = ToUVOverlay->AppendElement(FromUVOverlay->GetElement(FromElementID));
			IndexMaps.SetUV(UVLayerIndex, FromElementID, NewElementID);
		}
		return NewElementID;
	}

	static int AppendTriangleColorAttribute(const FDynamicMesh3* FromMesh, int FromElementID, FDynamicMesh3* ToMesh, FMeshIndexMappings& IndexMaps)
	{
		int NewElementID = IndexMaps.GetNewColor(FromElementID);
		if (NewElementID == IndexMaps.InvalidID())
		{
			const FDynamicMeshColorOverlay* FromOverlay = FromMesh->Attributes()->PrimaryColors();
			FDynamicMeshColorOverlay* ToOverlay = ToMesh->Attributes()->PrimaryColors();
			NewElementID = ToOverlay->AppendElement(FromOverlay->GetElement(FromElementID));
			IndexMaps.SetColor(FromElementID, NewElementID);
		}
		return NewElementID;
	}

	static int AppendTriangleNormalAttribute(const FDynamicMesh3* FromMesh, int FromElementID, FDynamicMesh3* ToMesh, int NormalLayerIndex, FMeshIndexMappings& IndexMaps)
	{
		int NewElementID = IndexMaps.GetNewNormal(NormalLayerIndex, FromElementID);
		if (NewElementID == IndexMaps.InvalidID())
		{
			const FDynamicMeshNormalOverlay* FromNormalOverlay = FromMesh->Attributes()->GetNormalLayer(NormalLayerIndex);
			FDynamicMeshNormalOverlay* ToNormalOverlay = ToMesh->Attributes()->GetNormalLayer(NormalLayerIndex);
			NewElementID = ToNormalOverlay->AppendElement(FromNormalOverlay->GetElement(FromElementID));
			IndexMaps.SetNormal(NormalLayerIndex, FromElementID, NewElementID);
		}
		return NewElementID;
	}

	static void AppendTriangleAttributes(const FDynamicMesh3* FromMesh, int FromTriangleID, FDynamicMesh3* ToMesh, int ToTriangleID, FMeshIndexMappings& IndexMaps, FDynamicMeshEditResult& ResultOut)
	{
		if (FromMesh->HasAttributes() == false || ToMesh->HasAttributes() == false)
		{
			return;
		}


		for (int UVLayerIndex = 0; UVLayerIndex < FMath::Min(FromMesh->Attributes()->NumUVLayers(), ToMesh->Attributes()->NumUVLayers()); UVLayerIndex++)
		{
			const FDynamicMeshUVOverlay* FromUVOverlay = FromMesh->Attributes()->GetUVLayer(UVLayerIndex);
			FDynamicMeshUVOverlay* ToUVOverlay = ToMesh->Attributes()->GetUVLayer(UVLayerIndex);
			if (FromUVOverlay->IsSetTriangle(FromTriangleID))
			{
				FIndex3i FromElemTri = FromUVOverlay->GetTriangle(FromTriangleID);
				FIndex3i ToElemTri = ToUVOverlay->GetTriangle(ToTriangleID);
				for (int j = 0; j < 3; ++j)
				{
					check(FromElemTri[j] != FDynamicMesh3::InvalidID);
					int NewElemID = AppendTriangleUVAttribute(FromMesh, FromElemTri[j], ToMesh, UVLayerIndex, IndexMaps);
					ToElemTri[j] = NewElemID;
				}
				ToUVOverlay->SetTriangle(ToTriangleID, ToElemTri);
			}
		}


		for (int NormalLayerIndex = 0; NormalLayerIndex < FMath::Min(FromMesh->Attributes()->NumNormalLayers(), ToMesh->Attributes()->NumNormalLayers()); NormalLayerIndex++)
		{
			const FDynamicMeshNormalOverlay* FromNormalOverlay = FromMesh->Attributes()->GetNormalLayer(NormalLayerIndex);
			FDynamicMeshNormalOverlay* ToNormalOverlay = ToMesh->Attributes()->GetNormalLayer(NormalLayerIndex);
			if (FromNormalOverlay->IsSetTriangle(FromTriangleID))
			{
				FIndex3i FromElemTri = FromNormalOverlay->GetTriangle(FromTriangleID);
				FIndex3i ToElemTri = ToNormalOverlay->GetTriangle(ToTriangleID);
				for (int j = 0; j < 3; ++j)
				{
					check(FromElemTri[j] != FDynamicMesh3::InvalidID);
					int NewElemID = AppendTriangleNormalAttribute(FromMesh, FromElemTri[j], ToMesh, NormalLayerIndex, IndexMaps);
					ToElemTri[j] = NewElemID;
				}
				ToNormalOverlay->SetTriangle(ToTriangleID, ToElemTri);
			}
		}

		if (FromMesh->Attributes()->HasPrimaryColors() && ToMesh->Attributes()->HasPrimaryColors())
		{
			const FDynamicMeshColorOverlay* FromOverlay = FromMesh->Attributes()->PrimaryColors();
			FDynamicMeshColorOverlay* ToOverlay = ToMesh->Attributes()->PrimaryColors();
			if (FromOverlay->IsSetTriangle(FromTriangleID))
			{
				FIndex3i FromElemTri = FromOverlay->GetTriangle(FromTriangleID);
				FIndex3i ToElemTri = ToOverlay->GetTriangle(ToTriangleID);
				for (int j = 0; j < 3; ++j)
				{
					check(FromElemTri[j] != FDynamicMesh3::InvalidID);
					int NewElemID = AppendTriangleColorAttribute(FromMesh, FromElemTri[j], ToMesh, IndexMaps);
					ToElemTri[j] = NewElemID;
				}
				ToOverlay->SetTriangle(ToTriangleID, ToElemTri);
			}
		}

		if (FromMesh->Attributes()->HasMaterialID() && ToMesh->Attributes()->HasMaterialID())
		{
			const FDynamicMeshMaterialAttribute* FromMaterialIDs = FromMesh->Attributes()->GetMaterialID();
			FDynamicMeshMaterialAttribute* ToMaterialIDs = ToMesh->Attributes()->GetMaterialID();
			ToMaterialIDs->SetValue(ToTriangleID, FromMaterialIDs->GetValue(FromTriangleID));
		}

		int NumPolygroupLayers = FMath::Min(FromMesh->Attributes()->NumPolygroupLayers(), ToMesh->Attributes()->NumPolygroupLayers());
		for (int PolygroupLayerIndex = 0; PolygroupLayerIndex < NumPolygroupLayers; PolygroupLayerIndex++)
		{
			// TODO: remap groups? this will be somewhat expensive...
			const FDynamicMeshPolygroupAttribute* FromPolygroups = FromMesh->Attributes()->GetPolygroupLayer(PolygroupLayerIndex);
			FDynamicMeshPolygroupAttribute* ToPolygroups = ToMesh->Attributes()->GetPolygroupLayer(PolygroupLayerIndex);
			ToPolygroups->SetValue(ToTriangleID, FromPolygroups->GetValue(FromTriangleID));
		}
	}

	static void AppendVertexAttributes(const FDynamicMesh3* FromMesh, FDynamicMesh3* ToMesh, FMeshIndexMappings& IndexMaps)
	{

		if (FromMesh->HasAttributes() == false || ToMesh->HasAttributes() == false)
		{
			return;
		}

		int NumWeightLayers = FMath::Min(FromMesh->Attributes()->NumWeightLayers(), ToMesh->Attributes()->NumWeightLayers());
		for (int WeightLayerIndex = 0; WeightLayerIndex < NumWeightLayers; WeightLayerIndex++)
		{
			const FDynamicMeshWeightAttribute* FromWeights = FromMesh->Attributes()->GetWeightLayer(WeightLayerIndex);
			FDynamicMeshWeightAttribute* ToWeights = ToMesh->Attributes()->GetWeightLayer(WeightLayerIndex);
			for (const TPair<int32, int32>& MapVID : IndexMaps.GetVertexMap().GetForwardMap())
			{
				float Weight;
				FromWeights->GetValue(MapVID.Key, &Weight);
				ToWeights->SetValue(MapVID.Value, &Weight);
			}
		}

		// Copy skin weight and generic attributes after full IndexMaps have been created. 	
		for (const TPair<FName, TUniquePtr<FDynamicMeshVertexSkinWeightsAttribute>>& AttribPair : FromMesh->Attributes()->GetSkinWeightsAttributes())
		{
			FDynamicMeshVertexSkinWeightsAttribute* ToAttrib = ToMesh->Attributes()->GetSkinWeightsAttribute(AttribPair.Key);
			if (ToAttrib)
			{
				ToAttrib->CopyThroughMapping(AttribPair.Value.Get(), IndexMaps);
			}
		}

		for (const TPair<FName, TUniquePtr<FDynamicMeshAttributeBase>>& AttribPair : FromMesh->Attributes()->GetAttachedAttributes())
		{
			if (ToMesh->Attributes()->HasAttachedAttribute(AttribPair.Key))
			{
				FDynamicMeshAttributeBase* ToAttrib = ToMesh->Attributes()->GetAttachedAttribute(AttribPair.Key);
				ToAttrib->CopyThroughMapping(AttribPair.Value.Get(), IndexMaps);
			}
		}
	}

	static bool SplitMesh(const FDynamicMesh3* InSourceMesh, TArray<FDynamicMesh3>& SplitMeshes,
		TFunctionRef<int(int)> TriIDToMeshID)
	{
		using namespace SplitMeshInternal;

		TMap<int, int> MeshIDToIndex;
		int NumMeshes = 0;
		bool bAlsoDelete = false;
		for (int TID : InSourceMesh->TriangleIndicesItr())
		{
			int MeshID = TriIDToMeshID(TID);

			if (!MeshIDToIndex.Contains(MeshID))
			{
				MeshIDToIndex.Add(MeshID, NumMeshes++);
			}
		}

		if (!bAlsoDelete && NumMeshes < 2)
		{
			return false; // nothing to do, so don't bother filling the split meshes array
		}

		SplitMeshes.Reset();
		SplitMeshes.SetNum(NumMeshes);
		// enable matching attributes
		for (FDynamicMesh3& M : SplitMeshes)
		{
			M.EnableMeshComponents(InSourceMesh->GetComponentsFlags());
			if (InSourceMesh->HasAttributes())
			{
				M.EnableAttributes();
				M.Attributes()->EnableMatchingAttributes(*InSourceMesh->Attributes(), false, false);
			}
		}

		if (NumMeshes == 0) // full delete case, just leave the empty mesh
		{
			return true;
		}

		TArray<FMeshIndexMappings> Mappings; Mappings.Reserve(NumMeshes);
		FDynamicMeshEditResult UnusedInvalidResultAccumulator; // only here because some functions require it
		for (int Idx = 0; Idx < NumMeshes; Idx++)
		{
			FMeshIndexMappings& Map = Mappings.Emplace_GetRef();
			Map.Initialize(&SplitMeshes[Idx]);
		}

		for (int SourceTID : InSourceMesh->TriangleIndicesItr())
		{
			int MeshID = TriIDToMeshID(SourceTID);

			int MeshIndex = MeshIDToIndex[MeshID];
			FDynamicMesh3& Mesh = SplitMeshes[MeshIndex];
			FMeshIndexMappings& IndexMaps = Mappings[MeshIndex];

			FIndex3i Tri = InSourceMesh->GetTriangle(SourceTID);

			// Find or create corresponding triangle group
			int NewGID = FDynamicMesh3::InvalidID;
			if (InSourceMesh->HasTriangleGroups())
			{
				int SourceGroupID = InSourceMesh->GetTriangleGroup(SourceTID);
				if (SourceGroupID >= 0)
				{
					NewGID = IndexMaps.GetNewGroup(SourceGroupID);
					if (NewGID == IndexMaps.InvalidID())
					{
						NewGID = Mesh.AllocateTriangleGroup();
						IndexMaps.SetGroup(SourceGroupID, NewGID);
					}
				}
			}

			bool bCreatedNewVertex[3] = { false, false, false };
			FIndex3i NewTri;
			for (int j = 0; j < 3; ++j)
			{
				int SourceVID = Tri[j];
				int NewVID = IndexMaps.GetNewVertex(SourceVID);
				if (NewVID == IndexMaps.InvalidID())
				{
					bCreatedNewVertex[j] = true;
					NewVID = Mesh.AppendVertex(*InSourceMesh, SourceVID);
					IndexMaps.SetVertex(SourceVID, NewVID);
				}
				NewTri[j] = NewVID;
			}

			int NewTID = Mesh.AppendTriangle(NewTri, NewGID);

			// conceivably this should never happen, but it did occur due to other mesh issues,
			// and it can be handled here without much effort
			if (NewTID < 0)
			{
				// append failed, try creating separate new vertices
				for (int j = 0; j < 3; ++j)
				{
					if (bCreatedNewVertex[j] == false)
					{
						int SourceVID = Tri[j];
						NewTri[j] = Mesh.AppendVertex(*InSourceMesh, SourceVID);
					}
				}
				NewTID = Mesh.AppendTriangle(NewTri, NewGID);
			}

			if (NewTID >= 0)
			{
				IndexMaps.SetTriangle(SourceTID, NewTID);
				AppendTriangleAttributes(InSourceMesh, SourceTID, &Mesh, NewTID, IndexMaps, UnusedInvalidResultAccumulator);
			}
			else
			{
				checkSlow(false);
				// something has gone very wrong, skip this triangle
			}
		}

		for (int Idx = 0; Idx < NumMeshes; Idx++)
		{
			AppendVertexAttributes(InSourceMesh, &SplitMeshes[Idx], Mappings[Idx]);
		}

		return true;
	}
}

void ADynamicMeshBaseActor::AdvancedPlaneCut(ADynamicMeshBaseActor* OtherMeshActor, FVector PlaneOrigin,
	FVector PlaneNormal, float CutUVScale)
{
	auto Start = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();

	SourceMesh.EnableAttributes();

	// 给三角形添加 Object Index 属性
	const FName ObjectIndexAttribute = "ObjectIndexAttribute";
	TDynamicMeshScalarTriangleAttribute<int>* SubObjectAttrib = new TDynamicMeshScalarTriangleAttribute<int>(&SourceMesh);
	SubObjectAttrib->SetName(ObjectIndexAttribute);
	SubObjectAttrib->Initialize(0);
	SourceMesh.Attributes()->AttachAttribute(ObjectIndexAttribute, SubObjectAttrib);

	// 从世界坐标转换到局部坐标
	FTransform LocalToWorld = GetTransform();
	FTransform WorldToLocal = LocalToWorld.Inverse();
	FVector LocalOrigin = WorldToLocal.TransformPosition(PlaneOrigin);
	FVector LocalNormal = WorldToLocal.TransformVector(PlaneNormal);

	// 使用相对更"原始"的MeshPlaneCut,可以保留更多信息
	TSharedPtr<FDynamicMesh3> ResultMesh = MakeShared<FDynamicMesh3>();
	ResultMesh->Copy(SourceMesh, true, true, true, true);

	FMeshPlaneCut Cut(ResultMesh.Get(), LocalOrigin, LocalNormal);
	Cut.UVScaleFactor = CutUVScale;
	Cut.bSimplifyAlongNewEdges = false;

	int MaxSubObjectID = -1;

	for (int TID : ResultMesh->TriangleIndicesItr())
	{
		MaxSubObjectID = FMath::Max(MaxSubObjectID, SubObjectAttrib->GetValue(TID));
	}


	Cut.CutWithoutDelete(true, 0, SubObjectAttrib, MaxSubObjectID + 1);
	Cut.HoleFill(ConstrainedDelaunayTriangulate<double>, true);
	Cut.TransferTriangleLabelsToHoleFillTriangles(SubObjectAttrib);

	// 初始化/设置IsShell属性，该属性会不断往下传递
	SetIsShell(*ResultMesh, Cut);

	// 根据三角形的SubObjectAttrib划分为两个SourceMesh
	TArray<FDynamicMesh3> SplitMeshes;
	bool bSucceeded = SplitMeshInternal::SplitMesh(ResultMesh.Get(), SplitMeshes, [SubObjectAttrib](int TID)
		{
			return SubObjectAttrib->GetValue(TID);
		});

	if (!bSucceeded)
		return;

	SplitMeshes[0].Attributes()->RemoveAttribute(ObjectIndexAttribute);
	SplitMeshes[1].Attributes()->RemoveAttribute(ObjectIndexAttribute);

	// 更新 "我的" Mesh
	NormalsMode = EDynamicMeshActorNormalsMode::SplitNormals;
	EditMesh([&](FDynamicMesh3& MeshToUpdate)
		{
			MeshToUpdate = MoveTemp(SplitMeshes[0]);
			RecomputeNormals(MeshToUpdate);
		});


	// 更新 "新的" Mesh
	if (IsValid(OtherMeshActor) && SplitMeshes.Num() == 2)
	{
		OtherMeshActor->NormalsMode = EDynamicMeshActorNormalsMode::SplitNormals;
		OtherMeshActor->SetActorLocation(this->GetActorLocation());
		OtherMeshActor->SetActorRotation(this->GetActorRotation());
		OtherMeshActor->EditMesh([&](FDynamicMesh3& MeshToUpdate)
			{
				MeshToUpdate = MoveTemp(SplitMeshes[1]);
				RecomputeNormals(MeshToUpdate);
			});
	}

	auto End = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();

	UE_LOG(LogTemp, Warning, TEXT("Plane Cut cost : %f ms"), End - Start);
}

UE_ENABLE_OPTIMIZATION
