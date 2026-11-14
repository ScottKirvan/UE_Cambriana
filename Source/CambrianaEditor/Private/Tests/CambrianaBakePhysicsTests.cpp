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
	// sk.ls: four unbranched branches of 20 bones under Root (SDD-rigging.md, Bone tree), so 4 chains (SDD-rigging.md, Physics),
	// one body each, holding a capsule per bone; Root has no body, so no body has one above it and there are no constraints.
	constexpr int32 SkBodies = 4;
	constexpr int32 SkCapsules = 80;

	// FF[+F]FF: chains {0,1} {2} {3,4} by hand from the chain definition, bodies on skeleton bones 1, 3 and 4; bones 2 and 3 are
	// children of bone 1, so the bodies on skeleton bones 3 and 4 each hang from the body on skeleton bone 1.
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
	// the length times thickness, SDD-surfacing.md; `!` thickness x 0.7, `'` length x 0.9). Bone 0 forks, so the chains are {0} {1} {2}
	// {3,4}, bodies on skeleton bones 1 to 4, and the bodies on skeleton bones 2, 3 and 4 hang from the body on skeleton bone 1.
	const TCHAR* const ThicknessGrammarText = TEXT("step 10cm\nthickness 20%\naxiom F [ + ! F ] [ - ' F ] F ! F\n");

	struct FExpectedBody
	{
		int32 SkeletonBone;
		/** By hand, SDD-rigging.md, Physics: r^4 / L for one bone, 1 / sum(L / r^4) along a chain. */
		double BendingStiffness;
		/** By hand: the capsules' volume, pi r^2 L + 4/3 pi r^3 each, in cm^3. */
		double Volume;
	};

	double CapsuleVolume(double Radius, double Length)
	{
		return UE_DOUBLE_PI * Radius * Radius * Length + 4.0 / 3.0 * UE_DOUBLE_PI * Radius * Radius * Radius;
	}

	TArray<FExpectedBody> ThicknessGrammarBodies()
	{
		return {
			{ 1, 1.0 / 10.0, CapsuleVolume(1.0, 10.0) },
			{ 2, 0.2401 / 10.0, CapsuleVolume(0.7, 10.0) },
			{ 3, 0.6561 / 9.0, CapsuleVolume(0.9, 9.0) },
			{ 4, 1.0 / (10.0 + 10.0 / 0.2401), CapsuleVolume(1.0, 10.0) + CapsuleVolume(0.7, 10.0) },
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
		Test.TestEqual(TEXT("Three constraints"), PhysicsAsset.ConstraintSetup.Num(), 3);
		Test.TestNull(TEXT("The body on skeleton bone 1 has nothing above it and no constraint"), FindConstraintOf(PhysicsAsset, Skeleton.GetBoneName(1)));
		for (int32 Body = 1; Body < Expected.Num(); ++Body)
		{
			const FString Label = FString::Printf(TEXT("Skeleton bone %d"), Expected[Body].SkeletonBone);
			const UPhysicsConstraintTemplate* Constraint = FindConstraintOf(PhysicsAsset, Skeleton.GetBoneName(Expected[Body].SkeletonBone));
			if (!Test.TestNotNull(*(Label + TEXT(": its body has a constraint")), Constraint))
			{
				continue;
			}
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
	"Cambriana.Editor.Bake.WritesTheSkeletalMeshsPhysicsAssetWithOneBodyPerChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsAssetTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakePhysics"));
	const FCambrianaBakeResult Result = Fixture.Bake();

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
	TestEqual(TEXT("One body per chain"), PhysicsAsset->SkeletalBodySetups.Num(), SkBodies);
	TestEqual(TEXT("A capsule per bone"), Capsules(*PhysicsAsset), SkCapsules);
	TestEqual(TEXT("No constraints: no body is above another"), PhysicsAsset->ConstraintSetup.Num(), 0);
	int32 NotOnARoot = 0;
	for (const USkeletalBodySetup* Body : PhysicsAsset->SkeletalBodySetups)
	{
		const int32 Bone = Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName);
		NotOnARoot += Bone == INDEX_NONE || Mesh->GetRefSkeleton().GetParentIndex(Bone) != 0;
	}
	TestEqual(TEXT("Each body is on a child of Root, the first bone of its chain"), NotOnARoot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaRebakePhysicsAssetTest,
	"Cambriana.Editor.Bake.RebakeRebuildsThePhysicsAssetInPlaceWithAConstraintFromEachBodyToTheBodyAboveIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaRebakePhysicsAssetTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("RebakePhysics"));
	if (!TestTrue(TEXT("The first bake succeeds"), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.GrammarText = ForkGrammarText; });

	const FCambrianaBakeResult Result = Fixture.Bake(AnswerYes());

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestTrue(TEXT("The same physics asset object is rebuilt"), PhysicsAsset && Fixture.FindPhysicsAsset() == PhysicsAsset && Result.PhysicsAsset == PhysicsAsset)
		|| !TestNotNull(TEXT("The skeletal mesh is there"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The skeletal mesh still uses it"), Mesh->GetPhysicsAsset() == PhysicsAsset);
	TestFalse(TEXT("It is saved"), PhysicsAsset->GetPackage()->IsDirty());
	TestEqual(TEXT("Its earlier bodies are replaced: three bodies"), PhysicsAsset->SkeletalBodySetups.Num(), 3);
	TestEqual(TEXT("A capsule per bone"), Capsules(*PhysicsAsset), 5);

	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const FName Trunk = Skeleton.GetBoneName(1);
	TSet<FName> Hanging;
	for (const UPhysicsConstraintTemplate* Constraint : PhysicsAsset->ConstraintSetup)
	{
		TestEqual(TEXT("Each constraint is to the trunk's body"), Constraint->DefaultInstance.ConstraintBone2, Trunk);
		Hanging.Add(Constraint->DefaultInstance.ConstraintBone1);
	}
	TestEqual(TEXT("Two constraints"), PhysicsAsset->ConstraintSetup.Num(), 2);
	TestTrue(TEXT("From the bodies on skeleton bones 3 and 4"), Hanging.Contains(Skeleton.GetBoneName(3)) && Hanging.Contains(Skeleton.GetBoneName(4)));
	const int32 TrunkBody = PhysicsAsset->FindBodyIndex(Trunk);
	TestFalse(TEXT("A constrained body does not collide with the body it hangs from"),
		PhysicsAsset->IsCollisionEnabled(TrunkBody, PhysicsAsset->FindBodyIndex(Skeleton.GetBoneName(3))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakePhysicsNameTakenTest,
	"Cambriana.Editor.Bake.AssetOfAnotherClassWithThePhysicsAssetNameIsLeftAndNothingIsBaked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakePhysicsNameTakenTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is not a physics asset; nothing is baked"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakePhysicsNameTaken"));
	UPackage* Package = CreatePackage(*Fixture.PhysicsAssetPackage);
	UCambrianaGrammar* Occupant = NewObject<UCambrianaGrammar>(Package, *(TEXT("PHYS_") + Fixture.Name), RF_Public | RF_Standalone);

	const FCambrianaBakeResult Result = Fixture.Bake(AnswerYes());

	TestTrue(TEXT("The bake reports the name taken"), Result.Status == ECambrianaBakeStatus::NameTaken);
	TestTrue(TEXT("The other asset is untouched"),
		FindObject<UObject>(nullptr, *(Fixture.PhysicsAssetPackage + TEXT(".PHYS_") + Fixture.Name)) == Occupant && IsValid(Occupant));
	TestNull(TEXT("No skeletal mesh is created"), Fixture.FindSkeletalMesh());
	TestFalse(TEXT("Nothing is saved"), Fixture.MeshFileExists() || Fixture.AnySkeletalFileExists() || Fixture.PhysicsAssetFileExists());
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
	if (!TestTrue(TEXT("The grammar bakes"), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("The physics asset is written"), PhysicsAsset) || !TestNotNull(TEXT("The skeletal mesh is written"), Mesh))
	{
		return false;
	}
	TestEqual(TEXT("Four bodies"), PhysicsAsset->SkeletalBodySetups.Num(), 4);
	TestDrives(*this, *PhysicsAsset, Mesh->GetRefSkeleton(), 1000.0);

	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Bake.PhysicsStiffness = 3000.0f; });
	if (!TestTrue(TEXT("The grammar rebakes"), Fixture.Bake(AnswerYes()).Status == ECambrianaBakeStatus::Baked))
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
	if (!TestTrue(TEXT("The grammar bakes"), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	const UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	const USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("The physics asset is written"), PhysicsAsset) || !TestNotNull(TEXT("The skeletal mesh is written"), Mesh))
	{
		return false;
	}
	TestEqual(TEXT("Four bodies"), PhysicsAsset->SkeletalBodySetups.Num(), 4);
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
		TestEqual(*(Label + TEXT(": mass from the capsules' volume, in kg")), double(Body.CalculateMass()), ExpectedMass, ExpectedMass * 1e-4);
	}
	return true;
}

#endif
