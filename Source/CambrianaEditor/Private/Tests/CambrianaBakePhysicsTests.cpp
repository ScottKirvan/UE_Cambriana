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

#endif
