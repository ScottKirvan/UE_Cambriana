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
	// SDD-rigging.md, Bone tree: sk.ls has 80 bones, 4 of them roots. Its four branches are each one unbranched run of 20 F with no
	// `]`, `f`, `z` or `g` inside it, so by the chain definition (SDD-rigging.md, Physics) it has 4 chains of 20 bones.
	constexpr int32 SkChains = 4;
	constexpr int32 SkBonesPerChain = 20;

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

	FString Describe(const TArray<TArray<int32>>& Chains)
	{
		TArray<FString> Parts;
		for (const TArray<int32>& Chain : Chains)
		{
			TArray<FString> Bones;
			for (const int32 Bone : Chain)
			{
				Bones.Add(FString::FromInt(Bone));
			}
			Parts.Add(TEXT("{") + FString::Join(Bones, TEXT(",")) + TEXT("}"));
		}
		return FString::Join(Parts, TEXT(" "));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaChainsTest,
	"Cambriana.PhysicsBodies.ChainsAreUnbranchedRunsOfDrawnSegmentsBrokenAtForksAndChainBreaks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaChainsTest::RunTest(const FString& Parameters)
{
	// Hand calculation from the chain definition. F draws bone 0, 1, 2 in order; [ and ] push and pop the current bone; f moves
	// without changing it but breaks the chain (SDD-rigging.md, Bone tree; SDD-surfacing.md, Tubes).
	// FFF: one unbranched run.
	TestEqual(TEXT("FFF"), Describe(Cambriana::FindChains(Build(Legacy(TEXT("FFF"))).Bones)), FString(TEXT("{0,1,2}")));
	// F[+F]F: bones 1 and 2 are both children of bone 0, so bone 0 forks and each bone is its own chain.
	TestEqual(TEXT("F[+F]F"), Describe(Cambriana::FindChains(Build(Legacy(TEXT("F[+F]F"))).Bones)), FString(TEXT("{0} {1} {2}")));
	// FF[+F]FF: the fork at bone 1 ends the chain {0,1}; bone 2 is the branch, and bones 3 and 4 run on from bone 1 after `]`.
	TestEqual(TEXT("FF[+F]FF"), Describe(Cambriana::FindChains(Build(Legacy(TEXT("FF[+F]FF"))).Bones)), FString(TEXT("{0,1} {2} {3,4}")));
	// FfF: bone 1 is bone 0's only child, but f breaks the chain before it.
	TestEqual(TEXT("FfF"), Describe(Cambriana::FindChains(Build(Legacy(TEXT("FfF"))).Bones)), FString(TEXT("{0} {1}")));

	const TArray<TArray<int32>> SkChainList = Cambriana::FindChains(Build(LoadFixture(TEXT("sk.ls"))).Bones);
	TestEqual(TEXT("sk.ls has 4 chains"), SkChainList.Num(), SkChains);
	for (const TArray<int32>& Chain : SkChainList)
	{
		TestEqual(TEXT("Each of 20 bones"), Chain.Num(), SkBonesPerChain);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPhysicsBodiesTest,
	"Cambriana.PhysicsBodies.OneBodyPerChainOnItsFirstBoneHoldsACapsuleAlongEachBone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPhysicsBodiesTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = Build(LoadFixture(TEXT("sk.ls")));
	const TArray<FCambrianaPhysicsBody> Bodies = Cambriana::MakePhysicsBodies(Result);
	if (!TestEqual(TEXT("sk.ls has one body per chain"), Bodies.Num(), SkChains))
	{
		return false;
	}
	const TArray<FTransform> Pose = ComponentSpacePose(Cambriana::MakeReferenceSkeleton(Result));
	int32 WrongCapsules = 0;
	for (const FCambrianaPhysicsBody& Body : Bodies)
	{
		const int32 First = Body.SkeletonBone - 1;
		if (!TestTrue(TEXT("The body is on a bone of the tree"), Result.Bones.IsValidIndex(First) && Pose.IsValidIndex(Body.SkeletonBone)))
		{
			return false;
		}
		TestEqual(TEXT("The body is on a root of the tree, the first bone of its chain"), Result.Bones[First].Parent, INDEX_NONE);
		TestEqual(TEXT("No body is above it: Root has none"), Body.ParentBodyBone, INDEX_NONE);
		if (!TestEqual(TEXT("A capsule per bone of the chain"), Body.Capsules.Num(), SkBonesPerChain))
		{
			continue;
		}
		// The chain's bones are the body's bone and each one's only child after it.
		int32 Bone = First;
		for (const FKSphylElem& Capsule : Body.Capsules)
		{
			const FCambrianaBone& Expected = Result.Bones[Bone];
			// SDD-rigging.md, Physics: one capsule per bone (length, radius). An FKSphylElem lies along its own Z axis.
			const FTransform InComponent = Capsule.GetTransform() * Pose[Body.SkeletonBone];
			const bool bCentred = FVector3d::Distance(InComponent.GetLocation(), (Expected.Start + Expected.End) * 0.5) <= Tolerance;
			const bool bAlong = InComponent.GetUnitAxis(EAxis::Z).Equals(Expected.Forward, Tolerance);
			const bool bSized = FMath::IsNearlyEqual(Capsule.Radius, Expected.Radius, Tolerance)
				&& FMath::IsNearlyEqual(Capsule.Length, FVector3d::Distance(Expected.Start, Expected.End), Tolerance);
			WrongCapsules += !(bCentred && bAlong && bSized);
			Bone = Result.Bones.IndexOfByPredicate([Bone](const FCambrianaBone& Child) { return Child.Parent == Bone; });
			if (Bone == INDEX_NONE)
			{
				break;
			}
		}
	}
	TestEqual(TEXT("Every capsule is centred on its bone, along it, with the bone's radius and length"), WrongCapsules, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPhysicsBodyParentsTest,
	"Cambriana.PhysicsBodies.EachBodyHangsFromTheNearestBodyAboveIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPhysicsBodyParentsTest::RunTest(const FString& Parameters)
{
	// FF[+F]FF: chains {0,1} {2} {3,4} (see the chains test), bodies on skeleton bones 1, 3 and 4 (Root is bone 0). Bones 2 and 3 are
	// children of bone 1, in the first chain, so both hang from the body on skeleton bone 1.
	const TArray<FCambrianaPhysicsBody> Bodies = Cambriana::MakePhysicsBodies(Build(Legacy(TEXT("FF[+F]FF"))));
	if (!TestEqual(TEXT("Three bodies"), Bodies.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Body 0 is on skeleton bone 1"), Bodies[0].SkeletonBone, 1);
	TestEqual(TEXT("It has no body above it"), Bodies[0].ParentBodyBone, INDEX_NONE);
	TestEqual(TEXT("It holds two capsules"), Bodies[0].Capsules.Num(), 2);
	TestEqual(TEXT("Body 1 is on skeleton bone 3"), Bodies[1].SkeletonBone, 3);
	TestEqual(TEXT("It hangs from the body on skeleton bone 1"), Bodies[1].ParentBodyBone, 1);
	TestEqual(TEXT("Body 2 is on skeleton bone 4"), Bodies[2].SkeletonBone, 4);
	TestEqual(TEXT("It hangs from the body on skeleton bone 1"), Bodies[2].ParentBodyBone, 1);
	TestEqual(TEXT("It holds two capsules"), Bodies[2].Capsules.Num(), 2);
	return true;
}

#endif
