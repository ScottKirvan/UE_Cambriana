#include "Animation/Skeleton.h"
#include "CambrianaBuild.h"
#include "CambrianaSkeletalMesh.h"
#include "CambrianaStaticMesh.h"
#include "CambrianaTestUtilities.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::ComponentSpacePose;
using Cambriana::Tests::LoadFixture;

namespace
{
	// SDD-rigging.md, Bone tree: sk.ls has 80 bones, 4 of them roots, all drawn segments.
	constexpr int32 SkBones = 80;
	constexpr int32 SkRoots = 4;

	// SDD-rigging.md, Bake skeleton: one synthetic Root bone parents the generated bone tree.
	constexpr int32 SkSkeletonBones = SkBones + 1;

	// SDD-surfacing.md, Tubes: sk.ls at 8 sides has 672 vertices and 1,280 triangles.
	constexpr int32 SkVertices = 672;
	constexpr int32 SkTriangles = 1280;

	// sk.ls draws its four branches in colours 1 to 4 (FMT-legacy-ls: `c` adds 1 to the colour, `[` pushes it).
	const TArray<int64> SkMaterialIds = { 1, 2, 3, 4 };

	// SDD-rigging.md, Pose: every bone's transform matches the core within 1e-4 cm.
	constexpr double BoneTolerance = 1e-4;

	FCambrianaBuildResult BuildSk()
	{
		FCambrianaBuildInput Input;
		Input.GrammarText = LoadFixture(TEXT("sk.ls"));
		Input.Seed = 12345;
		return Cambriana::BuildMesh(Input);
	}

	double DistanceToSegment(const FVector3d& Point, const FVector3d& Start, const FVector3d& End)
	{
		return FVector3d::Distance(Point, FMath::ClosestPointOnSegment(Point, Start, End));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildBonesTest,
	"Cambriana.SkeletalMesh.BuildCarriesTheCoreBoneTreeAndEachVertexInfluence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildBonesTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = BuildSk();
	if (!TestEqual(TEXT("sk.ls has 80 bones"), Result.Bones.Num(), SkBones))
	{
		return false;
	}
	TestEqual(TEXT("4 of them are roots"), Result.Bones.FilterByPredicate([](const FCambrianaBone& Bone) { return Bone.Parent == INDEX_NONE; }).Num(), SkRoots);

	// SDD-rigging.md, Bone names: symbol, `_`, depth-first index. sk.ls draws only F, so its names are F_0 to F_79.
	TSet<FName> Names;
	for (const FCambrianaBone& Bone : Result.Bones)
	{
		Names.Add(Bone.Name);
	}
	int32 Missing = 0;
	for (int32 Index = 0; Index < SkBones; ++Index)
	{
		Missing += !Names.Contains(FName(*FString::Printf(TEXT("F_%d"), Index)));
	}
	TestEqual(TEXT("Bone names F_0 to F_79 are each present"), Missing, 0);

	TestEqual(TEXT("Influences for every mesh vertex"), Result.VertexInfluences.Num(), Result.Mesh.MaxVertexID());
	TestEqual(TEXT("sk.ls has 672 vertices"), Result.Mesh.VertexCount(), SkVertices);
	int32 NotOneBone = 0;
	for (const FCambrianaVertexInfluences& Influences : Result.VertexInfluences)
	{
		// SDD-rigging.md, Skinning: tubes bind each vertex with weight 1 to the bone that placed it.
		NotOneBone += !(Influences.Num() == 1 && Influences[0].Weight == 1.0 && Result.Bones.IsValidIndex(Influences[0].Bone));
	}
	TestEqual(TEXT("Every vertex is bound with weight 1 to one bone"), NotOneBone, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReferenceSkeletonTest,
	"Cambriana.SkeletalMesh.ReferenceSkeletonIsRootParentingTheBoneTreeAtTheBonesOwnFrames",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaReferenceSkeletonTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = BuildSk();
	const FReferenceSkeleton Skeleton = Cambriana::MakeReferenceSkeleton(Result);
	if (!TestEqual(TEXT("80 bones plus Root"), Skeleton.GetNum(), SkSkeletonBones) || Result.Bones.Num() != SkBones)
	{
		return false;
	}
	TestEqual(TEXT("Bone 0 is Root"), Skeleton.GetBoneName(0), FName(TEXT("Root")));
	TestEqual(TEXT("Root has no parent"), Skeleton.GetParentIndex(0), INDEX_NONE);
	TestTrue(TEXT("Root is at the origin, unrotated"), Skeleton.GetRefBonePose()[0].Equals(FTransform::Identity, BoneTolerance));

	const TArray<FTransform> Pose = ComponentSpacePose(Skeleton);
	int32 RootChildren = 0;
	int32 WrongParent = 0;
	int32 WrongName = 0;
	int32 WrongPose = 0;
	for (int32 Index = 0; Index < Result.Bones.Num(); ++Index)
	{
		const FCambrianaBone& Bone = Result.Bones[Index];
		const int32 SkeletonIndex = Index + 1;
		const int32 ExpectedParent = Bone.Parent == INDEX_NONE ? 0 : Bone.Parent + 1;
		RootChildren += Skeleton.GetParentIndex(SkeletonIndex) == 0;
		WrongParent += Skeleton.GetParentIndex(SkeletonIndex) != ExpectedParent;
		WrongName += Skeleton.GetBoneName(SkeletonIndex) != Bone.Name;
		// SDD-rigging.md, Bake skeleton: the bind pose is the grammar's own values, the core's bones as built.
		const FTransform& Transform = Pose[SkeletonIndex];
		const bool bAtStart = FVector3d::Distance(Transform.GetLocation(), Bone.Start) <= BoneTolerance;
		const bool bXAlong = Transform.GetUnitAxis(EAxis::X).Equals(Bone.Forward, BoneTolerance);
		const bool bZUp = Transform.GetUnitAxis(EAxis::Z).Equals(Bone.Up, BoneTolerance);
		WrongPose += !(bAtStart && bXAlong && bZUp && FMath::IsNearlyEqual(Transform.GetScale3D().X, 1.0, BoneTolerance));
	}
	TestEqual(TEXT("Root parents the 4 roots of the tree"), RootChildren, SkRoots);
	TestEqual(TEXT("Every other bone's parent is its core parent"), WrongParent, 0);
	TestEqual(TEXT("Every bone keeps its core name"), WrongName, 0);
	TestEqual(TEXT("Every bone is at its start, X along the bone and Z up, unscaled"), WrongPose, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaInfluenceLimitTest,
	"Cambriana.SkeletalMesh.SkinWeightsKeepTheTwelveStrongestInfluencesRenormalised",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaInfluenceLimitTest::RunTest(const FString& Parameters)
{
	// SDD-rigging.md, Skinning: baking applies Unreal's per-vertex influence limit, keeping the strongest influences renormalised.
	// The engine's limit is MAX_TOTAL_INFLUENCES, 12. Thirteen influences on bones 0 to 12 weighing 1 to 13: bone 0, the weakest, is
	// dropped; the rest sum to 91 - 1 = 90, so bone b keeps (b + 1) / 90, on skeleton bone b + 1.
	TArray<FCambrianaInfluence> Influences;
	for (int32 Bone = 0; Bone < 13; ++Bone)
	{
		Influences.Add({ Bone, double(Bone + 1) });
	}
	const UE::AnimationCore::FBoneWeights Weights = Cambriana::MakeBoneWeights(Influences);
	if (!TestEqual(TEXT("12 influences kept"), Weights.Num(), 12))
	{
		return false;
	}
	// FBoneWeights stores weights in 16 bits.
	const double Quantum = 2.0 / 65535.0;
	for (int32 Bone = 1; Bone < 13; ++Bone)
	{
		const int32 Found = Weights.FindWeightIndexByBone(FBoneIndexType(Bone + 1));
		if (TestTrue(FString::Printf(TEXT("Bone %d is kept, on skeleton bone %d"), Bone, Bone + 1), Found != INDEX_NONE))
		{
			TestEqual(FString::Printf(TEXT("Bone %d weighs %d / 90"), Bone, Bone + 1), double(Weights[Found].GetWeight()), (Bone + 1) / 90.0, Quantum);
		}
	}
	TestEqual(TEXT("The weakest, bone 0, is dropped"), Weights.FindWeightIndexByBone(FBoneIndexType(1)), INDEX_NONE);

	const UE::AnimationCore::FBoneWeights Single = Cambriana::MakeBoneWeights({ FCambrianaInfluence{ 4, 1.0 } });
	TestTrue(TEXT("One influence of weight 1 on bone 4 is skeleton bone 5 at full weight"),
		Single.Num() == 1 && Single[0].GetBoneIndex() == 5 && Single[0].GetRawWeight() == UE::AnimationCore::FBoneWeight::GetMaxRawWeight());

	const UE::AnimationCore::FBoneWeights None = Cambriana::MakeBoneWeights({});
	TestTrue(TEXT("A vertex without influences is bound to Root at full weight"),
		None.Num() == 1 && None[0].GetBoneIndex() == 0 && None[0].GetRawWeight() == UE::AnimationCore::FBoneWeight::GetMaxRawWeight());
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaSkeletalMeshFromBuildTest,
	"Cambriana.SkeletalMesh.BuildResultMakesTheSkinnedMeshAndItsSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaSkeletalMeshFromBuildTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = BuildSk();
	USkeletalMesh* Mesh = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	USkeleton* Skeleton = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestTrue(TEXT("The skeletal mesh is made"), Cambriana::SetSkeletalMeshFromBuild(*Mesh, *Skeleton, Result)))
	{
		return false;
	}

	TestTrue(TEXT("The mesh uses the skeleton"), Mesh->GetSkeleton() == Skeleton);
	TestEqual(TEXT("The mesh has 80 bones plus Root"), Mesh->GetRefSkeleton().GetNum(), SkSkeletonBones);
	TestEqual(TEXT("The skeleton has 80 bones plus Root"), Skeleton->GetReferenceSkeleton().GetNum(), SkSkeletonBones);
	TestEqual(TEXT("The skeleton's first bone is Root"), Skeleton->GetReferenceSkeleton().GetBoneName(0), FName(TEXT("Root")));

	const TArray<FSkeletalMaterial>& Slots = Mesh->GetMaterials();
	if (TestEqual(TEXT("One material slot per material ID"), Slots.Num(), SkMaterialIds.Num()))
	{
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("Slot %d is named after material ID %lld"), Index, SkMaterialIds[Index]), Slots[Index].MaterialSlotName,
				FName(*FString::Printf(TEXT("Material_%lld"), SkMaterialIds[Index])));
		}
	}

	const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
	TestEqual(TEXT("Triangles"), RenderData && RenderData->LODRenderData.Num() > 0 ? int32(RenderData->LODRenderData[0].GetTotalFaces()) : 0, SkTriangles);

	const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
	if (!TestTrue(TEXT("The mesh has a LOD 0 model"), Model && Model->LODModels.Num() > 0))
	{
		return false;
	}
	// SDD-rigging.md, Skinning: each tube vertex is bound with weight 1 to the bone that placed it, so it lies on that bone's tube:
	// within the bone's radius of its segment.
	int32 Vertices = 0;
	int32 NotOneBone = 0;
	int32 OffTheBone = 0;
	for (const FSkelMeshSection& Section : Model->LODModels[0].Sections)
	{
		for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
		{
			++Vertices;
			const int32 SkeletonBone = Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[0]) ? Section.BoneMap[Vertex.InfluenceBones[0]] : INDEX_NONE;
			if (Vertex.InfluenceWeights[0] != UE::AnimationCore::FBoneWeight::GetMaxRawWeight() || SkeletonBone < 1 || SkeletonBone > SkBones)
			{
				++NotOneBone;
				continue;
			}
			const FCambrianaBone& Bone = Result.Bones[SkeletonBone - 1];
			OffTheBone += DistanceToSegment(FVector3d(Vertex.Position), Bone.Start, Bone.End) > Bone.Radius + BoneTolerance;
		}
	}
	TestTrue(TEXT("The mesh has vertices"), Vertices > 0);
	TestEqual(TEXT("Every vertex is bound at full weight to one bone of the tree"), NotOneBone, 0);
	TestEqual(TEXT("Every vertex is on the tube of its bone"), OffTheBone, 0);
	return true;
}
#endif

#endif
