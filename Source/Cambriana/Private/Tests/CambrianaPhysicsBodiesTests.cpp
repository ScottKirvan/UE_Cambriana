#include "CambrianaBuild.h"
#include "CambrianaPhysicsBodies.h"
#include "CambrianaSkeletalMesh.h"
#include "CambrianaTestUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::ComponentSpacePose;
using Cambriana::Tests::LoadFixture;

namespace
{
	// SDD-rigging.md, Bone tree: sk.ls has 80 bones, all drawn, 4 of them roots. SDD-rigging.md, Physics: one body per drawn bone.
	constexpr int32 SkBones = 80;

	constexpr double Tolerance = 1e-4;

	FCambrianaBuildResult Build(const FString& Text)
	{
		FCambrianaBuildInput Input;
		Input.GrammarText = Text;
		Input.Seed = 12345;
		return Cambriana::BuildMesh(Input);
	}

	FString Legacy(const TCHAR* Axiom)
	{
		return FString::Printf(TEXT("1\n25\n50\n%s\n@\n"), Axiom);
	}

	const FCambrianaPhysicsBody* FindBodyOn(const TArray<FCambrianaPhysicsBody>& Bodies, int32 SkeletonBone)
	{
		return Bodies.FindByPredicate([SkeletonBone](const FCambrianaPhysicsBody& Body) { return Body.SkeletonBone == SkeletonBone; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPhysicsBodiesTest,
	"Cambriana.PhysicsBodies.OneBodyPerDrawnBoneHoldsOneCapsuleAlongIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPhysicsBodiesTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = Build(LoadFixture(TEXT("sk.ls")));
	const TArray<FCambrianaPhysicsBody> Bodies = Cambriana::MakePhysicsBodies(Result);
	if (!TestEqual(TEXT("sk.ls has 80 bones"), Result.Bones.Num(), SkBones) || !TestEqual(TEXT("sk.ls has one body per drawn bone"), Bodies.Num(), SkBones))
	{
		return false;
	}
	const TArray<FTransform> Pose = ComponentSpacePose(Cambriana::MakeReferenceSkeleton(Result));
	int32 WrongCapsules = 0;
	for (int32 Bone = 0; Bone < SkBones; ++Bone)
	{
		// The reference skeleton puts Root at 0 and build bone i at i + 1 (Cambriana::MakeReferenceSkeleton).
		const FCambrianaPhysicsBody* Body = FindBodyOn(Bodies, Bone + 1);
		if (!TestNotNull(*FString::Printf(TEXT("Bone %d has a body"), Bone), Body)
			|| !TestEqual(*FString::Printf(TEXT("Bone %d's body holds one capsule"), Bone), Body->Capsules.Num(), 1))
		{
			continue;
		}
		const FCambrianaBone& Expected = Result.Bones[Bone];
		// SDD-rigging.md, Physics: the capsule is centred on the bone, along it, with its radius and length. An FKSphylElem lies along
		// its own Z axis.
		const FKSphylElem& Capsule = Body->Capsules[0];
		const FTransform InComponent = Capsule.GetTransform() * Pose[Bone + 1];
		const bool bCentred = FVector3d::Distance(InComponent.GetLocation(), (Expected.Start + Expected.End) * 0.5) <= Tolerance;
		const bool bAlong = InComponent.GetUnitAxis(EAxis::Z).Equals(Expected.Forward, Tolerance);
		const bool bSized = FMath::IsNearlyEqual(Capsule.Radius, Expected.Radius, Tolerance)
			&& FMath::IsNearlyEqual(Capsule.Length, FVector3d::Distance(Expected.Start, Expected.End), Tolerance);
		WrongCapsules += !(bCentred && bAlong && bSized);
	}
	TestEqual(TEXT("Every capsule is centred on its bone, along it, with the bone's radius and length"), WrongCapsules, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPhysicsBodyUndrawnTest,
	"Cambriana.PhysicsBodies.UndrawnPolygonBonesHaveNoBody",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPhysicsBodyUndrawnTest::RunTest(const FString& Parameters)
{
	// SDD-rigging.md, Bone tree: the two F are drawn bones and the polygon gets an undrawn, zero-length bone; only drawn bones get a body.
	const FCambrianaBuildResult Result = Build(Legacy(TEXT("F{.+f.+f.+f.}F")));
	TArray<int32> DrawnSkeletonBones;
	int32 Polygons = 0;
	for (int32 Bone = 0; Bone < Result.Bones.Num(); ++Bone)
	{
		if (Result.Bones[Bone].Kind == ECambrianaBoneKind::DrawnSegment)
		{
			DrawnSkeletonBones.Add(Bone + 1);
		}
		Polygons += Result.Bones[Bone].Kind == ECambrianaBoneKind::PolygonAttachment;
	}
	if (!TestEqual(TEXT("Two drawn bones"), DrawnSkeletonBones.Num(), 2) || !TestEqual(TEXT("One polygon bone"), Polygons, 1))
	{
		return false;
	}
	TArray<int32> BodySkeletonBones;
	for (const FCambrianaPhysicsBody& Body : Cambriana::MakePhysicsBodies(Result))
	{
		BodySkeletonBones.Add(Body.SkeletonBone);
	}
	BodySkeletonBones.Sort();
	TestEqual(TEXT("A body on each drawn bone and on nothing else"),
		FString::JoinBy(BodySkeletonBones, TEXT(","), [](int32 Bone) { return FString::FromInt(Bone); }),
		FString::JoinBy(DrawnSkeletonBones, TEXT(","), [](int32 Bone) { return FString::FromInt(Bone); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPhysicsBodyParentsTest,
	"Cambriana.PhysicsBodies.EachBodyHangsFromItsParentBonesBody",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPhysicsBodyParentsTest::RunTest(const FString& Parameters)
{
	// FF[+F]FF by hand (SDD-rigging.md, Bone tree): bone 1 is bone 0's child; [ pushes bone 1, so bone 2 is its child; ] pops back to
	// bone 1, so bone 3 is its child too, and bone 4 is bone 3's. In the reference skeleton bone i is at i + 1, under Root at 0.
	const TArray<FCambrianaPhysicsBody> Bodies = Cambriana::MakePhysicsBodies(Build(Legacy(TEXT("FF[+F]FF"))));
	if (!TestEqual(TEXT("Five bodies, one per bone"), Bodies.Num(), 5))
	{
		return false;
	}
	const int32 ExpectedParentBody[] = { INDEX_NONE, 1, 2, 2, 4 };
	for (int32 Bone = 0; Bone < 5; ++Bone)
	{
		const FCambrianaPhysicsBody* Body = FindBodyOn(Bodies, Bone + 1);
		if (TestNotNull(*FString::Printf(TEXT("Skeleton bone %d has a body"), Bone + 1), Body))
		{
			TestEqual(*FString::Printf(TEXT("Skeleton bone %d's body hangs from"), Bone + 1), Body->ParentBodyBone, ExpectedParentBody[Bone]);
		}
	}

	// A straight run of four bones: each bone's body hangs from the one before it; the first, a root under Root, has none.
	const TArray<FCambrianaPhysicsBody> Run = Cambriana::MakePhysicsBodies(Build(Legacy(TEXT("FFFF"))));
	if (!TestEqual(TEXT("A run of four bones has four bodies"), Run.Num(), 4))
	{
		return false;
	}
	for (int32 Bone = 0; Bone < 4; ++Bone)
	{
		const FCambrianaPhysicsBody* Body = FindBodyOn(Run, Bone + 1);
		if (TestNotNull(*FString::Printf(TEXT("Run bone %d has a body"), Bone), Body))
		{
			TestEqual(*FString::Printf(TEXT("Run bone %d's body hangs from"), Bone), Body->ParentBodyBone, Bone == 0 ? INDEX_NONE : Bone);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPhysicsBodyBendingStiffnessTest,
	"Cambriana.PhysicsBodies.BendingStiffnessIsTheBonesRadiusToTheFourthOverItsLength",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPhysicsBodyBendingStiffnessTest::RunTest(const FString& Parameters)
{
	// Hand calculation. A bone's radius is half its length times thickness (SDD-surfacing.md), `!` multiplies thickness by 0.7 and `'`
	// length by 0.9 (grammar-guide.md, command reference), so with step 10 cm and thickness 20%:
	// bone 0: L 10, r 1. Bone 1, after `!`: L 10, r 0.7. Bone 2, after `'`: L 9, r 0.9. Bone 3: L 10, r 1. Bone 4, after `!`: L 10, r 0.7.
	const FCambrianaBuildResult Result = Build(TEXT("step 10cm\nthickness 20%\naxiom F [ + ! F ] [ - ' F ] F ! F\n"));
	const TArray<FCambrianaPhysicsBody> Bodies = Cambriana::MakePhysicsBodies(Result);
	if (!TestEqual(TEXT("Five bones"), Result.Bones.Num(), 5) || !TestEqual(TEXT("Five bodies"), Bodies.Num(), 5))
	{
		return false;
	}
	const double Radii[] = { 1.0, 0.7, 0.9, 1.0, 0.7 };
	const double Lengths[] = { 10.0, 10.0, 9.0, 10.0, 10.0 };
	// r^4 / L: 1 / 10, 0.2401 / 10, 0.6561 / 9, 1 / 10, 0.2401 / 10.
	const double Expected[] = { 0.1, 0.02401, 0.0729, 0.1, 0.02401 };
	for (int32 Bone = 0; Bone < 5; ++Bone)
	{
		TestEqual(FString::Printf(TEXT("Bone %d's radius"), Bone), Result.Bones[Bone].Radius, Radii[Bone], Tolerance);
		TestEqual(FString::Printf(TEXT("Bone %d's length"), Bone), FVector3d::Distance(Result.Bones[Bone].Start, Result.Bones[Bone].End), Lengths[Bone], Tolerance);
		const FCambrianaPhysicsBody* Body = FindBodyOn(Bodies, Bone + 1);
		if (TestNotNull(FString::Printf(TEXT("Bone %d has a body"), Bone), Body))
		{
			TestEqual(FString::Printf(TEXT("Bone %d's body: r^4 / L"), Bone), Body->BendingStiffness, Expected[Bone], 1e-6);
		}
	}
	return true;
}

#endif
