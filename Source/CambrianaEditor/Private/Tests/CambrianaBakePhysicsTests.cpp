#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaGrammar.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// sk.ls: 80 drawn bones, 4 of them roots under Root (SDD-rigging.md, Bone tree). SDD-rigging.md, Physics: one body per drawn bone,
	// each holding one capsule, and a constraint from each body to its parent bone's body; Root has no body, so the 4 roots' bodies have
	// none: 80 - 4 = 76 constraints.
	constexpr int32 SkBodies = 80;
	constexpr int32 SkRoots = 4;
	constexpr int32 SkConstraints = SkBodies - SkRoots;

	// FF[+F]FF by hand (SDD-rigging.md, Bone tree): bone 1 is bone 0's child, bones 2 and 3 are bone 1's, bone 4 is bone 3's. In the
	// skeleton bone i is at i + 1 under Root, so the bodies on skeleton bones 2, 3, 4 and 5 hang from those on 1, 2, 2 and 4.
	const TCHAR* const ForkGrammarText = TEXT("1\n25\n50\nFF[+F]FF\n@\n");

	int32 Capsules(const UPhysicsAsset& PhysicsAsset)
	{
		int32 Count = 0;
		for (const USkeletalBodySetup* Body : PhysicsAsset.SkeletalBodySetups)
		{
			Count += Body ? Body->AggGeom.SphylElems.Num() : 0;
		}
		return Count;
	}

	// Step 10 cm, thickness 20%: bones 0 to 4 have length and radius 10 and 1, 10 and 0.7, 9 and 0.9, 10 and 1, 10 and 0.7 (radius half
	// the length times thickness, SDD-surfacing.md; `!` thickness x 0.7, `'` length x 0.9). Bones 1, 2 and 3 are children of bone 0 and
	// bone 4 of bone 3, so the bodies on skeleton bones 2, 3 and 4 hang from the body on skeleton bone 1, and the one on 5 from 4's.
	const TCHAR* const ThicknessGrammarText = TEXT("step 10cm\nthickness 20%\naxiom F [ + ! F ] [ - ' F ] F ! F\n");

	struct FExpectedBody
	{
		int32 SkeletonBone;
		/** The skeleton bone of the body it hangs from, or INDEX_NONE. */
		int32 ParentSkeletonBone;
		/** By hand, SDD-rigging.md, Physics: the bone's r^4 / L. */
		double BendingStiffness;
		/** By hand: the capsule's volume, pi r^2 L + 4/3 pi r^3, in cm^3. */
		double Volume;
	};

	double CapsuleVolume(double Radius, double Length)
	{
		return UE_DOUBLE_PI * Radius * Radius * Length + 4.0 / 3.0 * UE_DOUBLE_PI * Radius * Radius * Radius;
	}

	TArray<FExpectedBody> ThicknessGrammarBodies()
	{
		return {
			{ 1, INDEX_NONE, 1.0 / 10.0, CapsuleVolume(1.0, 10.0) },
			{ 2, 1, 0.2401 / 10.0, CapsuleVolume(0.7, 10.0) },
			{ 3, 1, 0.6561 / 9.0, CapsuleVolume(0.9, 9.0) },
			{ 4, 1, 1.0 / 10.0, CapsuleVolume(1.0, 10.0) },
			{ 5, 4, 0.2401 / 10.0, CapsuleVolume(0.7, 10.0) },
		};
	}

	const UPhysicsConstraintTemplate* FindConstraintOf(const UPhysicsAsset& PhysicsAsset, FName Bone)
	{
		const int32 Index = PhysicsAsset.FindConstraintIndex(Bone);
		return PhysicsAsset.ConstraintSetup.IsValidIndex(Index) ? PhysicsAsset.ConstraintSetup[Index].Get() : nullptr;
	}

	/** Checks each constrained body's drive against Stiffness times its hand-calculated bending stiffness. */
	void TestDrives(FAutomationTestBase& Test, const UPhysicsAsset& PhysicsAsset, const FReferenceSkeleton& Skeleton, double Stiffness)
	{
		const TArray<FExpectedBody> Expected = ThicknessGrammarBodies();
		Test.TestEqual(TEXT("Four constraints"), PhysicsAsset.ConstraintSetup.Num(), 4);
		Test.TestNull(TEXT("The body on skeleton bone 1 has nothing above it and no constraint"), FindConstraintOf(PhysicsAsset, Skeleton.GetBoneName(1)));
		for (int32 Body = 1; Body < Expected.Num(); ++Body)
		{
			const FString Label = FString::Printf(TEXT("Skeleton bone %d"), Expected[Body].SkeletonBone);
			const UPhysicsConstraintTemplate* Constraint = FindConstraintOf(PhysicsAsset, Skeleton.GetBoneName(Expected[Body].SkeletonBone));
			if (!Test.TestNotNull(*(Label + TEXT(": its body has a constraint")), Constraint))
			{
				continue;
			}
			Test.TestEqual(*(Label + TEXT(": to the body it hangs from")), Constraint->DefaultInstance.ConstraintBone2,
				Skeleton.GetBoneName(Expected[Body].ParentSkeletonBone));
			const FAngularDriveConstraint& Drive = Constraint->DefaultInstance.ProfileInstance.AngularDrive;
			const double ExpectedStiffness = Stiffness * Expected[Body].BendingStiffness;
			Test.TestTrue(*(Label + TEXT(": SLERP drive")), Drive.AngularDriveMode == EAngularDriveMode::SLERP);
			Test.TestTrue(*(Label + TEXT(": orientation drive on")), Drive.SlerpDrive.bEnablePositionDrive);
			Test.TestTrue(*(Label + TEXT(": velocity drive on, so its damping applies")), Drive.SlerpDrive.bEnableVelocityDrive);
			Test.TestFalse(*(Label + TEXT(": force mode, so the stiffness is a torque per angle")), Drive.bAccelerationMode);
			Test.TestTrue(*(Label + TEXT(": targets the bind pose")), Drive.OrientationTarget.IsZero() && Drive.AngularVelocityTarget.IsZero());
			Test.TestEqual(*(Label + TEXT(": stiffness")), double(Drive.SlerpDrive.Stiffness), ExpectedStiffness, ExpectedStiffness * 1e-5);
			// FConstraintDrive's defaults are stiffness 50 and damping 1 (ConstraintDrives.cpp).
			Test.TestEqual(*(Label + TEXT(": damping, 1/50 of the stiffness")), double(Drive.SlerpDrive.Damping), ExpectedStiffness / 50.0, ExpectedStiffness * 1e-7);
			Test.TestEqual(*(Label + TEXT(": no force limit")), Drive.SlerpDrive.MaxForce, 0.0f);
			Test.TestTrue(*(Label + TEXT(": the default profile holds the drive")),
				Constraint->GetConstraintProfilePropertiesOrDefault(NAME_None).AngularDrive.SlerpDrive.Stiffness == Drive.SlerpDrive.Stiffness);
		}
	}

	Cambriana::Editor::FAskToOverwrite AnswerYes()
	{
		return [](const FText&) { return EAppReturnType::Yes; };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsAssetTest,
	"Cambriana.Editor.Bake.WritesTheSkeletalMeshsPhysicsAssetWithOneBodyPerDrawnBoneJoinedToItsParentBonesBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsAssetTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysics"));
	const FCambrianaBakeResult Result = Fixture.BakeSkeletal();

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("A physics asset PHYS_BakePhysics is in the folder BakePhysics beside the grammar"), PhysicsAsset)
		|| !TestNotNull(TEXT("The skeletal mesh is written"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The result is that physics asset"), Result.PhysicsAsset == PhysicsAsset);
	TestTrue(TEXT("The skeletal mesh uses it"), Mesh->GetPhysicsAsset() == PhysicsAsset);
	TestTrue(TEXT("It previews on the skeletal mesh"), PhysicsAsset->GetPreviewMesh() == Mesh);
	TestTrue(TEXT("It is saved to disk"), Fixture.PhysicsAssetFileExists());
	TestFalse(TEXT("Its package has no unsaved changes"), PhysicsAsset->GetPackage()->IsDirty());
	TestEqual(TEXT("One body per drawn bone"), PhysicsAsset->SkeletalBodySetups.Num(), SkBodies);
	TestEqual(TEXT("One capsule per body"), Capsules(*PhysicsAsset), SkBodies);
	TestEqual(TEXT("A constraint at every bone but the four under Root"), PhysicsAsset->ConstraintSetup.Num(), SkConstraints);

	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	int32 NotToItsParent = 0;
	for (const UPhysicsConstraintTemplate* Constraint : PhysicsAsset->ConstraintSetup)
	{
		const int32 Bone = Skeleton.FindBoneIndex(Constraint->DefaultInstance.ConstraintBone1);
		NotToItsParent += Bone == INDEX_NONE || Skeleton.FindBoneIndex(Constraint->DefaultInstance.ConstraintBone2) != Skeleton.GetParentIndex(Bone);
	}
	TestEqual(TEXT("Each constraint joins a bone's body to its parent bone's body"), NotToItsParent, 0);
	int32 UnconstrainedBelowRoot = 0;
	for (const USkeletalBodySetup* Body : PhysicsAsset->SkeletalBodySetups)
	{
		const int32 Bone = Skeleton.FindBoneIndex(Body->BoneName);
		UnconstrainedBelowRoot += PhysicsAsset->FindConstraintIndex(Body->BoneName) == INDEX_NONE && (Bone == INDEX_NONE || Skeleton.GetParentIndex(Bone) != 0);
	}
	TestEqual(TEXT("Only the bodies on Root's children have no constraint"), UnconstrainedBelowRoot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsIterationCountsTest,
	"Cambriana.Editor.Bake.EachBodyTakesThePlacedComponentsSolverIterationCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsIterationCountsTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysicsIterations"));
	if (!TestTrue(TEXT("The bake succeeds"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	const UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	if (!TestNotNull(TEXT("The physics asset is written"), PhysicsAsset))
	{
		return false;
	}
	// SDD-rigging.md, Physics: each body overrides the solver's iteration counts, so that USkeletalMeshComponent::InitArticulated's
	// copy of the component's counts to every body applies (SkeletalMeshComponentPhysics.cpp). The counts stay FBodyInstance's
	// defaults, position 8, velocity 2 and projection 1 (BodyInstance.cpp); a body without the override reports -1.
	int32 NotOverridden = 0;
	for (const USkeletalBodySetup* Body : PhysicsAsset->SkeletalBodySetups)
	{
		const FBodyInstance& Instance = Body->DefaultInstance;
		NotOverridden += Instance.GetPositionSolverIterationCount() != 8 || Instance.GetVelocitySolverIterationCount() != 2
			|| Instance.GetProjectionSolverIterationCount() != 1;
	}
	TestEqual(TEXT("Every body overrides the iteration counts with the engine defaults"), NotOverridden, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaRebakePhysicsAssetTest,
	"Cambriana.Editor.Bake.RebakeRebuildsThePhysicsAssetInPlaceWithAConstraintFromEachBodyToTheBodyAboveIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaRebakePhysicsAssetTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("RebakePhysics"));
	if (!TestTrue(TEXT("The first bake succeeds"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.GrammarText = ForkGrammarText; });

	const FCambrianaBakeResult Result = Fixture.BakeSkeletal(AnswerYes());

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestTrue(TEXT("The same physics asset object is rebuilt"), PhysicsAsset && Fixture.FindPhysicsAsset() == PhysicsAsset && Result.PhysicsAsset == PhysicsAsset)
		|| !TestNotNull(TEXT("The skeletal mesh is there"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The skeletal mesh still uses it"), Mesh->GetPhysicsAsset() == PhysicsAsset);
	TestFalse(TEXT("It is saved"), PhysicsAsset->GetPackage()->IsDirty());
	TestEqual(TEXT("Its earlier bodies are replaced: five bodies"), PhysicsAsset->SkeletalBodySetups.Num(), 5);
	TestEqual(TEXT("One capsule per body"), Capsules(*PhysicsAsset), 5);

	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	TestEqual(TEXT("Four constraints"), PhysicsAsset->ConstraintSetup.Num(), 4);
	TestNull(TEXT("The body on skeleton bone 1 has none"), FindConstraintOf(*PhysicsAsset, Skeleton.GetBoneName(1)));
	const int32 ExpectedPairs[][2] = { { 2, 1 }, { 3, 2 }, { 4, 2 }, { 5, 4 } };
	for (const auto& Pair : ExpectedPairs)
	{
		const FString Label = FString::Printf(TEXT("Skeleton bone %d"), Pair[0]);
		const UPhysicsConstraintTemplate* Constraint = FindConstraintOf(*PhysicsAsset, Skeleton.GetBoneName(Pair[0]));
		if (!TestNotNull(*(Label + TEXT(" has a constraint")), Constraint))
		{
			continue;
		}
		TestEqual(*(Label + FString::Printf(TEXT(": to the body on skeleton bone %d"), Pair[1])), Constraint->DefaultInstance.ConstraintBone2,
			Skeleton.GetBoneName(Pair[1]));
		TestFalse(*(Label + TEXT(": its body does not collide with the body it hangs from")),
			PhysicsAsset->IsCollisionEnabled(PhysicsAsset->FindBodyIndex(Skeleton.GetBoneName(Pair[0])), PhysicsAsset->FindBodyIndex(Skeleton.GetBoneName(Pair[1]))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsNameTakenTest,
	"Cambriana.Editor.Bake.BakeToSkeletalMeshReplacesAnAssetOfAnotherClassWithThePhysicsAssetNameOnYes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsNameTakenTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysicsNameTaken"));
	UPackage* Package = CreatePackage(*Fixture.PhysicsAssetPackage);
	const TWeakObjectPtr<UObject> Occupant = NewObject<UCambrianaGrammar>(Package, *(TEXT("PHYS_") + Fixture.Name), RF_Public | RF_Standalone);

	TArray<FString> Questions;
	const FCambrianaBakeResult Result = Fixture.BakeSkeletal(AnswerYes(), [&Questions](const FText& Question) { Questions.Add(Question.ToString()); return EAppReturnType::Yes; });

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("Replacing is asked once, naming the physics asset"), Questions.Num() == 1 && Questions[0].EndsWith(Fixture.PhysicsAssetPackage));
	TestFalse(TEXT("The other asset is deleted"), Occupant.IsValid());
	TestNotNull(TEXT("The skeletal mesh is created"), Fixture.FindSkeletalMesh());
	TestNotNull(TEXT("A physics asset is at the name"), Fixture.FindPhysicsAsset());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsStiffnessTest,
	"Cambriana.Editor.Bake.EachConstraintDrivesItsBodyToTheBindPoseWithStiffnessFromThicknessScaledByTheBakeSetting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsStiffnessTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysicsStiffness"));
	Fixture.Grammar->GrammarText = ThicknessGrammarText;
	Fixture.Grammar->Bake.PhysicsStiffness = 1000.0f;
	if (!TestTrue(TEXT("The grammar bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("The physics asset is written"), PhysicsAsset) || !TestNotNull(TEXT("The skeletal mesh is written"), Mesh))
	{
		return false;
	}
	TestEqual(TEXT("Five bodies"), PhysicsAsset->SkeletalBodySetups.Num(), 5);
	TestDrives(*this, *PhysicsAsset, Mesh->GetRefSkeleton(), 1000.0);

	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Bake.PhysicsStiffness = 3000.0f; });
	if (!TestTrue(TEXT("The grammar rebakes"), Fixture.BakeSkeletal(AnswerYes()).Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	AddInfo(TEXT("Rebaked with three times the overall stiffness"));
	TestDrives(*this, *PhysicsAsset, Mesh->GetRefSkeleton(), 3000.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsMassTest,
	"Cambriana.Editor.Bake.EachBodysMassComesFromItsCapsulesVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsMassTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysicsMass"));
	Fixture.Grammar->GrammarText = ThicknessGrammarText;
	if (!TestTrue(TEXT("The grammar bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	const UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	const USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("The physics asset is written"), PhysicsAsset) || !TestNotNull(TEXT("The skeletal mesh is written"), Mesh))
	{
		return false;
	}
	TestEqual(TEXT("Five bodies"), PhysicsAsset->SkeletalBodySetups.Num(), 5);
	for (const FExpectedBody& Expected : ThicknessGrammarBodies())
	{
		const FString Label = FString::Printf(TEXT("Skeleton bone %d"), Expected.SkeletonBone);
		const int32 Index = PhysicsAsset->FindBodyIndex(Mesh->GetRefSkeleton().GetBoneName(Expected.SkeletonBone));
		if (!TestTrue(*(Label + TEXT(" has a body")), PhysicsAsset->SkeletalBodySetups.IsValidIndex(Index)))
		{
			continue;
		}
		const USkeletalBodySetup& Body = *PhysicsAsset->SkeletalBodySetups[Index];
		TestFalse(*(Label + TEXT(": mass is not overridden")), Body.DefaultInstance.bOverrideMass);
		TestEqual(*(Label + TEXT(": mass scale 1")), Body.DefaultInstance.MassScale, 1.0f);
		// UBodySetup::CalculateMass (BodySetup.cpp), as the runtime's ApplyMassPropertiesModifiers (BodyUtils.cpp): (volume x density) ^
		// RaiseMassToPower, with UPhysicalMaterial's defaults density 1 g/cm^3 and RaiseMassToPower 0.75 (PhysicalMaterial.cpp).
		const double ExpectedMass = FMath::Pow(Expected.Volume * 0.001, 0.75);
		TestEqual(*(Label + TEXT(": mass from the capsule's volume, in kg")), double(Body.CalculateMass()), ExpectedMass, ExpectedMass * 1e-4);
	}
	return true;
}

#endif
