#include "Algo/AllOf.h"
#include "Animation/SkeletalMeshActor.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EditorReimportHandler.h"
#include "FileHelpers.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// SDD-rigging.md, Bone names: the drawing symbol, `_`, and the depth-first index; Bake skeleton: Root parents the tree.
	// FMT-grammar.md, Commands: F draws a segment and Z half of one, each a bone (docs/unreal/baking.md, Bones).
	/** Root, F_0. */
	const TCHAR* const OneBoneText = TEXT("step 10cm\n\naxiom F\n");
	/** Root, F_0, F_1 below F_0. */
	const TCHAR* const TwoBonesText = TEXT("step 10cm\n\naxiom F F\n");
	/** Root, F_0, F_1 below F_0, F_2 below F_1. */
	const TCHAR* const ThreeBonesText = TEXT("step 10cm\n\naxiom F F F\n");
	/** Root, F_0, Z_1 below F_0. */
	const TCHAR* const BoneThenHalfText = TEXT("step 10cm\n\naxiom F Z\n");
	/** Root, Z_0, F_1 below Z_0: F_1's parent is not the F_0 that TwoBonesText puts above it, so the bones cannot merge. */
	const TCHAR* const HalfThenBoneText = TEXT("step 10cm\n\naxiom Z F\n");

	bool Save(UObject* Asset)
	{
		return UEditorLoadingAndSavingUtils::SavePackages({ Asset->GetPackage() }, false);
	}

	/** The fixture's grammar holding Text, saved, and baked to a skeletal mesh. */
	bool BakeSkeletalFrom(FAutomationTestBase& Test, FBakeFixture& Fixture, const TCHAR* Text)
	{
		Fixture.Grammar->GrammarText = Text;
		return Test.TestTrue(FString::Printf(TEXT("%s: the grammar saves"), *Fixture.Name), Save(Fixture.Grammar))
			&& Test.TestTrue(FString::Printf(TEXT("%s: the skeletal mesh bakes"), *Fixture.Name), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked)
			&& Test.TestNotNull(FString::Printf(TEXT("%s: SK_%s"), *Fixture.Name, *Fixture.Name), Fixture.FindSkeletalMesh())
			&& Test.TestNotNull(FString::Printf(TEXT("%s: SKEL_%s"), *Fixture.Name, *Fixture.Name), Fixture.FindSkeleton())
			&& Test.TestNotNull(FString::Printf(TEXT("%s: PHYS_%s"), *Fixture.Name, *Fixture.Name), Fixture.FindPhysicsAsset());
	}

	void ChangeText(UCambrianaGrammar& Grammar, const TCHAR* Text)
	{
		Grammar.Modify();
		Grammar.GrammarText = Text;
	}

	/** Each bone's name and its parent's, NAME_None for a root, in bone order. */
	TArray<TPair<FName, FName>> BonesOf(const FReferenceSkeleton& Skeleton)
	{
		TArray<TPair<FName, FName>> Bones;
		for (int32 Bone = 0; Bone < Skeleton.GetRawBoneNum(); ++Bone)
		{
			const int32 Parent = Skeleton.GetRawParentIndex(Bone);
			Bones.Emplace(Skeleton.GetBoneName(Bone), Parent == INDEX_NONE ? NAME_None : Skeleton.GetBoneName(Parent));
		}
		return Bones;
	}

	FString Describe(const TArray<TPair<FName, FName>>& Bones)
	{
		TArray<FString> Parts;
		for (const TPair<FName, FName>& Bone : Bones)
		{
			Parts.Add(FString::Printf(TEXT("%s<%s"), *Bone.Key.ToString(), *Bone.Value.ToString()));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	/** Whether the skeleton holds exactly the bones given, each below the parent given, in any order. */
	bool HasExactlyBones(const FReferenceSkeleton& Skeleton, const TArray<TPair<FName, FName>>& Expected)
	{
		const TArray<TPair<FName, FName>> Bones = BonesOf(Skeleton);
		return Bones.Num() == Expected.Num()
			&& Algo::AllOf(Expected, [&Bones](const TPair<FName, FName>& Bone) { return Bones.Contains(Bone); });
	}

	TPair<FName, FName> Bone(const TCHAR* Name, const TCHAR* Parent)
	{
		return { FName(Name), Parent ? FName(Parent) : NAME_None };
	}

	TArray<FName> BodyBones(const UPhysicsAsset& PhysicsAsset)
	{
		TArray<FName> Bones;
		for (const USkeletalBodySetup* Body : PhysicsAsset.SkeletalBodySetups)
		{
			Bones.Add(Body ? Body->BoneName : NAME_None);
		}
		return Bones;
	}

	bool HasExactlyBodies(const UPhysicsAsset& PhysicsAsset, const TArray<FName>& Expected)
	{
		const TArray<FName> Bones = BodyBones(PhysicsAsset);
		return Bones.Num() == Expected.Num() && Algo::AllOf(Expected, [&Bones](FName Name) { return Bones.Contains(Name); });
	}

	/** Answers the merge question with Answer, counting the questions and keeping the last. */
	struct FMergeQuestion
	{
		explicit FMergeQuestion(EAppReturnType::Type InAnswer) : Answer(InAnswer) {}

		Cambriana::Editor::FAskToRecreateSkeleton Asker()
		{
			return [this](const FText& Question) { ++Asked; LastQuestion = Question.ToString(); return Answer; };
		}

		EAppReturnType::Type Answer;
		int32 Asked = 0;
		FString LastQuestion;
	};

	const TCHAR* AnswerName(EAppReturnType::Type Answer)
	{
		return Answer == EAppReturnType::Yes ? TEXT("Yes") : Answer == EAppReturnType::No ? TEXT("No") : TEXT("Cancel");
	}
}

// ATS-49: a skeleton shared with another skeletal mesh keeps its bones and gains the grammar's new ones.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportMergesIntoSharedSkeletonTest,
	"Cambriana.Editor.Reimport.ReimportMergesTheGrammarsNewBonesIntoASharedSkeletonKeepingEveryBoneItHadWithoutAsking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportMergesIntoSharedSkeletonTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportSharedSkeleton"));
	FBakeFixture Other(TEXT("ReimportSharedSkeletonOther"));
	if (!BakeSkeletalFrom(*this, Fixture, OneBoneText) || !BakeSkeletalFrom(*this, Other, BoneThenHalfText))
	{
		return false;
	}
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	USkeleton* Shared = Fixture.FindSkeleton();
	USkeletalMesh* OtherMesh = Other.FindSkeletalMesh();
	// The other mesh is given the grammar's skeleton as the FBX import merges each other mesh using a skeleton into it
	// (FbxSkeletalMeshImport.cpp, MergeAllBonesToBoneTree on each skeletal mesh using the skeleton) and sets it as its skeleton.
	if (!TestTrue(TEXT("The other mesh's bones merge into the grammar's skeleton"), Shared->MergeAllBonesToBoneTree(OtherMesh)))
	{
		return false;
	}
	OtherMesh->SetSkeleton(Shared);
	const TArray<TPair<FName, FName>> SharedBones = { Bone(TEXT("Root"), nullptr), Bone(TEXT("F_0"), TEXT("Root")), Bone(TEXT("Z_1"), TEXT("F_0")) };
	if (!TestTrue(FString::Printf(TEXT("The shared skeleton holds Root, F_0 and the other mesh's Z_1 (%s)"), *Describe(BonesOf(Shared->GetReferenceSkeleton()))),
		HasExactlyBones(Shared->GetReferenceSkeleton(), SharedBones)))
	{
		return false;
	}
	ASkeletalMeshActor* OtherPlaced = PlaceAsset<ASkeletalMeshActor>(OtherMesh);
	ACambrianaLiveActor* OtherLive = PlaceInCurrentLevel(Other.Grammar);
	ON_SCOPE_EXIT
	{
		if (OtherPlaced)
		{
			OtherPlaced->GetWorld()->EditorDestroyActor(OtherPlaced, false);
		}
		DestroyActor(OtherLive);
	};
	if (!TestNotNull(TEXT("The other mesh is placed in the level"), OtherPlaced) || !TestNotNull(TEXT("A live actor is placed from the other grammar"), OtherLive))
	{
		return false;
	}
	ChangeText(*Fixture.Grammar, TwoBonesText);

	FMergeQuestion Question(EAppReturnType::No);
	const FCambrianaBakeResult Result = Cambriana::Editor::RebakeMesh(*Fixture.Grammar, *Mesh, Question.Asker());

	TestTrue(TEXT("Reimport rebuilds the mesh"), Result.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("No question is asked"), Question.Asked, 0);
	TestTrue(TEXT("The mesh keeps the shared skeleton, the same object"), Mesh->GetSkeleton() == Shared && OtherMesh->GetSkeleton() == Shared);
	const TArray<TPair<FName, FName>> Merged = { Bone(TEXT("Root"), nullptr), Bone(TEXT("F_0"), TEXT("Root")), Bone(TEXT("Z_1"), TEXT("F_0")), Bone(TEXT("F_1"), TEXT("F_0")) };
	TestTrue(FString::Printf(TEXT("The shared skeleton keeps Root, F_0 and Z_1 and gains F_1 below F_0 (%s)"), *Describe(BonesOf(Shared->GetReferenceSkeleton()))),
		HasExactlyBones(Shared->GetReferenceSkeleton(), Merged));
	const TArray<TPair<FName, FName>> MeshBones = { Bone(TEXT("Root"), nullptr), Bone(TEXT("F_0"), TEXT("Root")), Bone(TEXT("F_1"), TEXT("F_0")) };
	TestTrue(FString::Printf(TEXT("The mesh's own bones are the grammar's: Root, F_0, F_1 (%s)"), *Describe(BonesOf(Mesh->GetRefSkeleton()))),
		HasExactlyBones(Mesh->GetRefSkeleton(), MeshBones));
	TestTrue(TEXT("The other mesh is compatible with the shared skeleton"), Shared->IsCompatibleMesh(OtherMesh));

	USkeletalMeshComponent* Component = OtherPlaced->GetSkeletalMeshComponent();
	Component->RefreshBoneTransforms();
	const FTriangleMatch Match = MatchTriangles(*OtherLive->GetDynamicMeshComponent()->GetMesh(),
		SkinnedTrianglesOf(*Component, OtherLive->GetDynamicMeshComponent()->GetComponentTransform()));
	TestTrue(FString::Printf(TEXT("The other mesh still renders on the shared skeleton as its grammar builds (%s)"), *Match.Describe()), Match.Matches());
	return true;
}

// ATS-49: bones that cannot merge ask before the skeleton is recreated; No and Cancel change nothing, Yes recreates it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportAsksBeforeRecreatingSkeletonTest,
	"Cambriana.Editor.Reimport.ReimportAsksBeforeRecreatingASkeletonTheGrammarsBonesCannotMergeIntoAndNoOrCancelChangesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportAsksBeforeRecreatingSkeletonTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportRecreateSkeleton"));
	if (!BakeSkeletalFrom(*this, Fixture, TwoBonesText))
	{
		return false;
	}
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	USkeleton* Skeleton = Fixture.FindSkeleton();
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	const FGuid SkeletonGuid = Skeleton->GetGuid();
	const TArray<TPair<FName, FName>> Baked = { Bone(TEXT("Root"), nullptr), Bone(TEXT("F_0"), TEXT("Root")), Bone(TEXT("F_1"), TEXT("F_0")) };
	// SDD-rigging.md: one body per drawn bone.
	const TArray<FName> BakedBodies = { FName(TEXT("F_0")), FName(TEXT("F_1")) };
	ChangeText(*Fixture.Grammar, HalfThenBoneText);

	for (const EAppReturnType::Type Answer : { EAppReturnType::No, EAppReturnType::Cancel })
	{
		const FString Label = AnswerName(Answer);
		FMergeQuestion Question(Answer);
		const FCambrianaBakeResult Result = Cambriana::Editor::RebakeMesh(*Fixture.Grammar, *Mesh, Question.Asker());

		TestEqual(*(Label + TEXT(": the question is asked once")), Question.Asked, 1);
		// The engine's question (FbxSkeletalMeshImport.cpp, SkeletonFailed_BoneMerge), with its warning about animation data.
		TestTrue(*(Label + TEXT(": it is the engine's failed-merge question")), Question.LastQuestion.Contains(TEXT("FAILED TO MERGE BONES")));
		TestTrue(*(Label + TEXT(": it warns that animation data may be invalidated")), Question.LastQuestion.Contains(TEXT("THIS MAY INVALIDATE OR REQUIRE RECOMPRESSION OF ANIMATION DATA")));
		TestTrue(*(Label + TEXT(": nothing is rebuilt")), Result.Status == ECambrianaBakeStatus::SkeletonRecreateDeclined);
		TestTrue(*(Label + TEXT(": the mesh, skeleton and physics asset are left without unsaved changes")),
			!Mesh->GetPackage()->IsDirty() && !Skeleton->GetPackage()->IsDirty() && !PhysicsAsset->GetPackage()->IsDirty());
		TestTrue(FString::Printf(TEXT("%s: the mesh keeps its bones Root, F_0, F_1 (%s)"), *Label, *Describe(BonesOf(Mesh->GetRefSkeleton()))),
			HasExactlyBones(Mesh->GetRefSkeleton(), Baked));
		TestTrue(FString::Printf(TEXT("%s: the skeleton keeps its bones Root, F_0, F_1 (%s)"), *Label, *Describe(BonesOf(Skeleton->GetReferenceSkeleton()))),
			HasExactlyBones(Skeleton->GetReferenceSkeleton(), Baked));
		TestTrue(*(Label + TEXT(": the skeleton is not recreated")), Skeleton->GetGuid() == SkeletonGuid);
		TestTrue(*(Label + TEXT(": the physics asset keeps its bodies on F_0 and F_1")), HasExactlyBodies(*PhysicsAsset, BakedBodies));
		TestTrue(*(Label + TEXT(": the mesh keeps its skeleton and physics asset")), Mesh->GetSkeleton() == Skeleton && Mesh->GetPhysicsAsset() == PhysicsAsset);
	}

	if (FApp::IsUnattended())
	{
		// With -unattended the engine's message dialog gives its default answer without showing; the Reimport it stops fails.
		bool bReimported = true;
		RunWithoutSavePrompt([&] { bReimported = FReimportManager::Instance()->Reimport(Mesh, false, false, FString(), nullptr, INDEX_NONE, false, true); });
		TestFalse(TEXT("Unattended, the engine's Reimport does not recreate the skeleton"), bReimported);
		TestTrue(TEXT("Unattended, the skeleton keeps its bones"), HasExactlyBones(Skeleton->GetReferenceSkeleton(), Baked) && !Skeleton->GetPackage()->IsDirty());
	}

	FMergeQuestion Yes(EAppReturnType::Yes);
	const FCambrianaBakeResult Result = Cambriana::Editor::RebakeMesh(*Fixture.Grammar, *Mesh, Yes.Asker());
	TestEqual(TEXT("Yes: the question is asked once"), Yes.Asked, 1);
	TestTrue(TEXT("Yes: the mesh is rebuilt"), Result.Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("Yes: the mesh keeps its skeleton, the same object"), Mesh->GetSkeleton() == Skeleton && Fixture.FindSkeleton() == Skeleton);
	const TArray<TPair<FName, FName>> Recreated = { Bone(TEXT("Root"), nullptr), Bone(TEXT("Z_0"), TEXT("Root")), Bone(TEXT("F_1"), TEXT("Z_0")) };
	TestTrue(FString::Printf(TEXT("Yes: the skeleton's bone tree is recreated as the grammar's: Root, Z_0, F_1 below Z_0 (%s)"), *Describe(BonesOf(Skeleton->GetReferenceSkeleton()))),
		HasExactlyBones(Skeleton->GetReferenceSkeleton(), Recreated));
	TestTrue(TEXT("Yes: the grammar's own physics asset is rebuilt with bodies on Z_0 and F_1"), HasExactlyBodies(*PhysicsAsset, { FName(TEXT("Z_0")), FName(TEXT("F_1")) }));
	return true;
}

// ATS-49: a physics asset that is not the grammar's own PHYS_<name> is never rebuilt.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportKeepsUserPhysicsAssetTest,
	"Cambriana.Editor.Reimport.ReimportLeavesAPhysicsAssetTheUserAssignedAloneAndRebuildsTheGrammarsOwn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportKeepsUserPhysicsAssetTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportUserPhysics"));
	if (!BakeSkeletalFrom(*this, Fixture, TwoBonesText))
	{
		return false;
	}
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	UPhysicsAsset* Own = Fixture.FindPhysicsAsset();
	// Beside the grammar's own, in the same bake folder, under a name of the user's.
	const FString UserPackageName = FPackageName::GetLongPackagePath(Fixture.PhysicsAssetPackage) / TEXT("PA_UserReimportUserPhysics");
	UPhysicsAsset* User = DuplicateObject<UPhysicsAsset>(Own, CreatePackage(*UserPackageName), TEXT("PA_UserReimportUserPhysics"));
	ON_SCOPE_EXIT
	{
		if (UPackage* Package = FindPackage(nullptr, *UserPackageName))
		{
			UnloadPackage(Package);
		}
	};
	if (!TestNotNull(TEXT("The user's physics asset is made"), User))
	{
		return false;
	}
	Mesh->SetPhysicsAsset(User);
	User->GetPackage()->SetDirtyFlag(false);
	Own->GetPackage()->SetDirtyFlag(false);
	const TArray<FName> TwoBodies = { FName(TEXT("F_0")), FName(TEXT("F_1")) };
	// SDD-rigging.md: one body per drawn bone; F F F draws F_0, F_1 and F_2.
	const TArray<FName> ThreeBodies = { FName(TEXT("F_0")), FName(TEXT("F_1")), FName(TEXT("F_2")) };
	ChangeText(*Fixture.Grammar, ThreeBonesText);

	FMergeQuestion Question(EAppReturnType::No);
	TestTrue(TEXT("Reimport rebuilds the mesh with the user's physics asset"),
		Cambriana::Editor::RebakeMesh(*Fixture.Grammar, *Mesh, Question.Asker()).Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("The mesh keeps the user's physics asset"), Mesh->GetPhysicsAsset() == User);
	TestTrue(TEXT("The user's physics asset keeps its two bodies"), HasExactlyBodies(*User, TwoBodies));
	TestFalse(TEXT("The user's physics asset is left without unsaved changes"), User->GetPackage()->IsDirty());
	TestTrue(TEXT("The grammar's own physics asset, not the mesh's, keeps its two bodies"), HasExactlyBodies(*Own, TwoBodies));
	TestFalse(TEXT("The grammar's own physics asset is left without unsaved changes"), Own->GetPackage()->IsDirty());

	Mesh->SetPhysicsAsset(Own);
	TestTrue(TEXT("Reimport rebuilds the mesh with the grammar's own physics asset"),
		Cambriana::Editor::RebakeMesh(*Fixture.Grammar, *Mesh, Question.Asker()).Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("The mesh keeps the grammar's own physics asset"), Mesh->GetPhysicsAsset() == Own);
	TestTrue(TEXT("The grammar's own physics asset is rebuilt with bodies on F_0, F_1 and F_2"), HasExactlyBodies(*Own, ThreeBodies));
	TestTrue(TEXT("The user's physics asset still has its two bodies"), HasExactlyBodies(*User, TwoBodies));
	TestEqual(TEXT("No question is asked"), Question.Asked, 0);
	return true;
}

#endif
