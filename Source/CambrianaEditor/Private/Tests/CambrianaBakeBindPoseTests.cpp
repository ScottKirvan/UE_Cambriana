#include "Animation/SkeletalMeshActor.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Elements/Actor/ActorElementData.h"
#include "Misc/AutomationTest.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Subsystems/PlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	ASkeletalMeshActor* PlaceSkeletalMesh(USkeletalMesh* Mesh)
	{
		UE::AssetPlacementUtil::FExtraPlaceAssetOptions Options;
		Options.bSelectOutput = false;
		const TArray<FTypedElementHandle> Placed = UE::AssetPlacementUtil::PlaceAssetInCurrentLevel(Mesh, Options);
		return Placed.Num() == 1 ? Cast<ASkeletalMeshActor>(ActorElementDataUtil::GetActorFromHandle(Placed[0])) : nullptr;
	}

	/** The triangles of the placed skeletal mesh's LOD 0, skinned on the CPU at its pose, in the space of the frame given. */
	TArray<FMeshTriangle> SkinnedTrianglesOf(USkeletalMeshComponent& Component, const FTransform& Frame)
	{
		TArray<FMeshTriangle> Triangles;
		const FSkeletalMeshRenderData* RenderData = Component.GetSkeletalMeshAsset() ? Component.GetSkeletalMeshAsset()->GetResourceForRendering() : nullptr;
		if (!RenderData || RenderData->LODRenderData.IsEmpty())
		{
			return Triangles;
		}
		const FSkeletalMeshLODRenderData& Lod = RenderData->LODRenderData[0];
		if (Component.GetComponentSpaceTransforms().Num() != Component.GetSkeletalMeshAsset()->GetRefSkeleton().GetNum())
		{
			// A placed skeletal mesh component poses itself when it ticks; the test does not tick, so it poses it now.
			Component.RefreshBoneTransforms();
		}
		TArray<FMatrix44f> RefToLocals;
		Component.CacheRefToLocalMatrices(RefToLocals);
		TArray<FVector3f> Positions;
		USkinnedMeshComponent::ComputeSkinnedPositions(&Component, Positions, RefToLocals, Lod, Lod.SkinWeightVertexBuffer);
		TArray<uint32> Indices;
		Lod.MultiSizeIndexContainer.GetIndexBuffer(Indices);
		const FTransform& ToWorld = Component.GetComponentTransform();
		for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
		{
			FMeshTriangle& Triangle = Triangles.AddDefaulted_GetRef();
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVector3d World = ToWorld.TransformPosition(FVector3d(Positions[Indices[Index + Corner]]));
				Triangle.Corners[Corner] = Frame.InverseTransformPosition(World);
			}
		}
		return Triangles;
	}

	/** ATS-42: bakes the fixture grammar, places its skeletal mesh and a live actor from it; the skinned bind pose must match the actor. */
	void TestBindPoseMatchesPlacedActor(FAutomationTestBase& Test, const TCHAR* AssetName, const TCHAR* Label, const FString& GrammarText)
	{
		FBakeFixture Fixture(AssetName);
		Fixture.Grammar->GrammarText = GrammarText;
		Fixture.Grammar->Seed = 7177;
		if (!Test.TestTrue(FString::Printf(TEXT("%s bakes"), Label), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
		{
			return;
		}
		ASkeletalMeshActor* Baked = PlaceSkeletalMesh(Fixture.FindSkeletalMesh());
		ACambrianaLiveActor* Live = PlaceInCurrentLevel(Fixture.Grammar);
		if (Test.TestNotNull(TEXT("The baked skeletal mesh is placed"), Baked) && Test.TestNotNull(TEXT("A live actor is placed from the grammar"), Live))
		{
			UDynamicMeshComponent* LiveComponent = Live->GetDynamicMeshComponent();
			const TArray<FMeshTriangle> Skinned = SkinnedTrianglesOf(*Baked->GetSkeletalMeshComponent(), LiveComponent->GetComponentTransform());
			const FTriangleMatch Match = MatchTriangles(*LiveComponent->GetMesh(), Skinned);
			Test.TestTrue(FString::Printf(TEXT("%s: the baked mesh at bind pose matches the live actor's triangles and vertices within 0.001 cm (%s)"),
				Label, *Match.Describe()), Match.Matches());
		}
		if (Baked)
		{
			Baked->GetWorld()->EditorDestroyActor(Baked, false);
		}
		DestroyActor(Live);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBindPoseMatchesLiveActorTest,
	"Cambriana.Editor.Bake.BakedSkeletalMeshAtBindPoseMatchesTheLiveActorBuiltFromTheGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBindPoseMatchesLiveActorTest::RunTest(const FString& Parameters)
{
	TestBindPoseMatchesPlacedActor(*this, TEXT("BindPoseSk"), TEXT("sk.ls"), LoadFile(FixturePath(TEXT("sk.ls"))));
	TestBindPoseMatchesPlacedActor(*this, TEXT("BindPoseSkLsys"), TEXT("sk.lsys"), LoadFile(FixturePath(TEXT("sk.lsys"))));
	TestBindPoseMatchesPlacedActor(*this, TEXT("BindPoseFork"), TEXT("fork"), TEXT("1\n25\n50\nFF[+F]FF\n@\n"));
	TestBindPoseMatchesPlacedActor(*this, TEXT("BindPoseZeroThickness"), TEXT("zero thickness"), TEXT("1\n25\n50\nF!(0)F\n@\n"));
	// About 0.00002 cm between neighbouring ring corners at the base, as in the static mesh comparison (CambrianaBakeMatchTests.cpp).
	TestBindPoseMatchesPlacedActor(*this, TEXT("BindPoseNearZeroThickness"), TEXT("near-zero thickness"), TEXT("1\n25\n50\n!(0.0000005)F\n@\n"));
	return true;
}

#endif
