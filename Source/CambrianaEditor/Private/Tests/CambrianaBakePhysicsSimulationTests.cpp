#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaGrammar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PreviewScene.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// A straight run of five bones, each 10 cm long with radius 1 cm (radius half the length times thickness, SDD-surfacing.md). In the
	// skeleton bone i is at i + 1 under Root (Cambriana::MakeReferenceSkeleton).
	const TCHAR* const RunGrammarText = TEXT("step 10cm\nthickness 20%\naxiom F F F F F\n");
	constexpr int32 RunBones = 5;
	constexpr double RunBoneLength = 10.0;

	constexpr float TickSeconds = 1.0f / 60.0f;
	constexpr int32 Ticks = 120;

	struct FRunMotion
	{
		bool bSimulated = false;
		/** The most the last bone's body went down, in cm. */
		double MaxDrop = 0.0;
		/** The most the last bone's body moved from where it started, in cm. */
		double MaxDeviation = 0.0;
		/** The most the distance from the first bone's body to the last grew, in cm: a run that comes apart grows. */
		double MaxStretch = 0.0;
		/** The position solver iterations the last bone's body runs with, or -1 for the solver's default. */
		int32 LastBodyPositionIterations = -1;
	};

	/**
	 * Places the baked mesh in a preview scene simulating physics, as the physics asset editor's preview does, with the run laid
	 * level, the first bone's body kinematic, and the rest simulated for Ticks steps under gravity. PositionIterations above 0 is set
	 * as the component's Position Solver Iteration Count, which USkeletalMeshComponent::InitArticulated copies to every body
	 * (SkeletalMeshComponentPhysics.cpp).
	 */
	FRunMotion SimulateLevelRun(USkeletalMesh& Mesh, UPhysicsAsset& PhysicsAsset, int32 PositionIterations)
	{
		FRunMotion Motion;
		const FReferenceSkeleton& Skeleton = Mesh.GetRefSkeleton();
		const FName FirstBone = Skeleton.GetBoneName(1);
		const FName LastBone = Skeleton.GetBoneName(RunBones);
		const int32 FirstBody = PhysicsAsset.FindBodyIndex(FirstBone);
		if (FirstBody == INDEX_NONE)
		{
			return Motion;
		}
		PhysicsAsset.SkeletalBodySetups[FirstBody]->PhysicsType = EPhysicsType::PhysType_Kinematic;

		FPreviewScene Scene(FPreviewScene::ConstructionValues().ShouldSimulatePhysics(true));
		USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(GetTransientPackage());
		// The collision profile a placed skeletal mesh actor gives its component (SkeletalMeshActor.cpp); without collision it has no bodies.
		Component->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		if (PositionIterations > 0)
		{
			Component->BodyInstance.SetOverrideIterationCounts(true);
			Component->BodyInstance.SetPositionSolverIterationCount(uint8(PositionIterations));
		}
		Component->SetSkeletalMesh(&Mesh);
		Scene.AddComponent(Component, FTransform::Identity);
		FBodyInstance* First = Component->GetBodyInstance(FirstBone);
		FBodyInstance* Last = Component->GetBodyInstance(LastBone);
		if (!First || !Last)
		{
			Scene.RemoveComponent(Component);
			return Motion;
		}
		const FVector Along = (Last->GetUnrealWorldTransform().GetLocation() - First->GetUnrealWorldTransform().GetLocation()).GetSafeNormal();
		Component->SetWorldRotation(FQuat::FindBetweenNormals(Along, FVector::XAxisVector), false, nullptr, ETeleportType::TeleportPhysics);
		const FVector LastStart = Last->GetUnrealWorldTransform().GetLocation();
		const double BindSpan = FVector::Distance(LastStart, First->GetUnrealWorldTransform().GetLocation());
		Motion.LastBodyPositionIterations = Last->GetPositionSolverIterationCount();

		Component->SetSimulatePhysics(true);
		Motion.bSimulated = Component->IsSimulatingPhysics(LastBone) && !Component->IsSimulatingPhysics(FirstBone);
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Scene.GetWorld()->Tick(LEVELTICK_All, TickSeconds);
			// A tick function runs once per frame counter, so each step advances it, as FTestWorldWrapper::TickTestWorld does (AutomationCommon.cpp).
			++GFrameCounter;
			const FVector LastNow = Last->GetUnrealWorldTransform().GetLocation();
			Motion.MaxDrop = FMath::Max(Motion.MaxDrop, LastStart.Z - LastNow.Z);
			Motion.MaxDeviation = FMath::Max(Motion.MaxDeviation, FVector::Distance(LastNow, LastStart));
			Motion.MaxStretch = FMath::Max(Motion.MaxStretch, FVector::Distance(LastNow, First->GetUnrealWorldTransform().GetLocation()) - BindSpan);
		}
		Scene.RemoveComponent(Component);
		return Motion;
	}

	/** Bakes the run with the Physics Stiffness given and simulates it level; false when it does not bake. */
	bool BakeAndSimulateRun(FAutomationTestBase& Test, const TCHAR* Name, float Stiffness, FRunMotion& Motion, int32 PositionIterations = 0)
	{
		FBakeFixture Fixture(Name);
		Fixture.Grammar->GrammarText = RunGrammarText;
		Fixture.Grammar->Bake.PhysicsStiffness = Stiffness;
		UPhysicsAsset* PhysicsAsset = nullptr;
		USkeletalMesh* Mesh = nullptr;
		if (!Test.TestTrue(TEXT("The run bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked)
			|| !Test.TestNotNull(TEXT("The physics asset is written"), PhysicsAsset = Fixture.FindPhysicsAsset())
			|| !Test.TestNotNull(TEXT("The skeletal mesh is written"), Mesh = Fixture.FindSkeletalMesh()))
		{
			return false;
		}
		Motion = SimulateLevelRun(*Mesh, *PhysicsAsset, PositionIterations);
		Test.AddInfo(FString::Printf(TEXT("Physics Stiffness %g: the last bone's body dropped at most %.3f cm, moved at most %.3f cm, the run stretched at most %.3f cm"),
			Stiffness, Motion.MaxDrop, Motion.MaxDeviation, Motion.MaxStretch));
		return Test.TestTrue(TEXT("The first bone's body is kinematic and the last bone's simulates"), Motion.bSimulated);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsRunTest,
	"Cambriana.Editor.Bake.AStraightRunOfBonesGetsABodyPerBoneAndAConstraintFromEachToTheOneBefore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsRunTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysicsRun"));
	Fixture.Grammar->GrammarText = RunGrammarText;
	if (!TestTrue(TEXT("The run bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	const UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	const USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("The physics asset is written"), PhysicsAsset) || !TestNotNull(TEXT("The skeletal mesh is written"), Mesh))
	{
		return false;
	}
	// SDD-rigging.md, Physics: N bones, N bodies; each but the first, under Root, joined to the one before it: N - 1 constraints.
	TestEqual(TEXT("Five bodies"), PhysicsAsset->SkeletalBodySetups.Num(), RunBones);
	TestEqual(TEXT("Four constraints"), PhysicsAsset->ConstraintSetup.Num(), RunBones - 1);
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	for (int32 Bone = 1; Bone <= RunBones; ++Bone)
	{
		const FString Label = FString::Printf(TEXT("Skeleton bone %d"), Bone);
		TestNotEqual(*(Label + TEXT(" has a body")), PhysicsAsset->FindBodyIndex(Skeleton.GetBoneName(Bone)), int32(INDEX_NONE));
		const int32 Constraint = PhysicsAsset->FindConstraintIndex(Skeleton.GetBoneName(Bone));
		if (Bone == 1)
		{
			TestEqual(*(Label + TEXT(" has no constraint")), Constraint, int32(INDEX_NONE));
		}
		else if (TestTrue(*(Label + TEXT(" has a constraint")), PhysicsAsset->ConstraintSetup.IsValidIndex(Constraint)))
		{
			TestEqual(*(Label + TEXT(": to the bone before it")), PhysicsAsset->ConstraintSetup[Constraint]->DefaultInstance.ConstraintBone2,
				Skeleton.GetBoneName(Bone - 1));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsRunBendsTest,
	"Cambriana.Editor.Bake.ALevelRunHeldAtItsFirstBoneWithoutStiffnessBendsDownAtItsJoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsRunBendsTest::RunTest(const FString& Parameters)
{
	FRunMotion Motion;
	if (!BakeAndSimulateRun(*this, TEXT("BakePhysicsRunBends"), 0.0f, Motion))
	{
		return false;
	}
	// By hand: with no drive, each joint swings to its 45 degree limit (FConeConstraint's default) or to hanging straight down, so
	// over 2 s the last bone's body, 40 cm out from the held one, falls far more than one bone length (10 cm). A run that cannot bend
	// stays level and does not drop at all.
	TestTrue(TEXT("The last bone's body drops more than one bone length"), Motion.MaxDrop > RunBoneLength);
	// The joints hold the run together: it bends rather than falls apart.
	TestTrue(TEXT("The run does not come apart: it stretches less than 1 cm"), Motion.MaxStretch < 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsRunStiffTest,
	"Cambriana.Editor.Bake.ALevelRunHeldAtItsFirstBoneWithAVeryHighStiffnessStaysNearItsBindPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsRunStiffTest::RunTest(const FString& Parameters)
{
	FRunMotion Motion;
	// ATS-82: the placed component's Position Solver Iteration Count raised to 32. At the solver's default of 8 the run's joints do
	// not converge and even locked joints leave the tip about 2 cm low.
	constexpr int32 PositionIterations = 32;
	if (!BakeAndSimulateRun(*this, TEXT("BakePhysicsRunStiff"), 1.0e8f, Motion, PositionIterations))
	{
		return false;
	}
	TestEqual(TEXT("The last bone's body runs the component's position solver iterations"), Motion.LastBodyPositionIterations, PositionIterations);
	// By hand (SDD-rigging.md, Physics): each capsule is 35.6 cm^3 (pi x 1 x 10 + 4/3 pi), mass (0.0356)^0.75 = 0.082 kg. The first
	// joint carries four bodies at 5, 15, 25 and 35 cm: torque 0.082 x 980 x 80 = 6400 kg cm^2/s^2. Its drive stiffness is
	// 1e8 x 1^4 / 10 = 1e7, so it turns about 6400 / 1e7 = 6.4e-4 rad, moving the last body 40 cm out about 0.03 cm; the other joints
	// carry less. Even doubled by overshoot that is far under 1 cm.
	TestTrue(TEXT("The last bone's body stays within 1 cm of its bind pose"), Motion.MaxDeviation < 1.0);
	return true;
}

#endif
