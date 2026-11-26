#include "Animation/SkeletalMeshActor.h"
#include "CambrianaAssetImportData.h"
#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaBuild.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Elements/Actor/ActorElementData.h"
#include "IContentBrowserSingleton.h"
#include "IDetailTreeNode.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// sk.ls draws four branches, one segment per generation (SDD-surfacing.md, Tubes: 2 triangles per side per segment, no
	// caps). With recursion overridden to 6 and sides to 5: 4 * 6 segments * 2 * 5 = 240 triangles; the grammar's own 20
	// generations at 8 sides would give 1,280.
	constexpr double OverriddenRecursion = 6.0;
	constexpr int32 OverriddenSides = 5;
	constexpr int32 OverriddenTriangles = 4 * 6 * 2 * 5;
	// Each segment drawn is one bone (SDD-rigging.md, Bone tree): 4 branches * 6 segments.
	constexpr int32 OverriddenBones = 4 * 6;

	void CollectButtons(const TSharedRef<SWidget>& Root, TArray<TSharedPtr<SButton>>& Buttons)
	{
		if (Root->GetType() == TEXT("SButton"))
		{
			Buttons.Add(StaticCastSharedRef<SButton>(Root));
			return;
		}
		FChildren* Children = Root->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			CollectButtons(Children->GetChildAt(Index), Buttons);
		}
	}

	FString LabelOf(const TSharedPtr<SButton>& Button)
	{
		const TSharedPtr<SWidget> Text = FindWidgetOfType(Button.ToSharedRef(), TEXT("STextBlock"));
		return Text ? StaticCastSharedPtr<STextBlock>(Text)->GetText().ToString() : FString();
	}

	/** The buttons of each row of the live actor's Details Cambriana category that has any, in row order. */
	TArray<TArray<TSharedPtr<SButton>>> DetailsButtonRows(UObject* Object)
	{
		TArray<TArray<TSharedPtr<SButton>>> ButtonRows;
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Object });
		const TSharedPtr<IDetailTreeNode> Category = FindCategory(*Rows, TEXT("Cambriana"));
		if (!Category)
		{
			return ButtonRows;
		}
		TArray<TSharedRef<IDetailTreeNode>> Nodes;
		Category->GetChildren(Nodes, true);
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const FNodeWidgets Widgets = Node->CreateNodeWidgets();
			TArray<TSharedPtr<SButton>> Buttons;
			for (const TSharedPtr<SWidget>& Root : { Widgets.NameWidget, Widgets.ValueWidget, Widgets.WholeRowWidget })
			{
				if (Root)
				{
					CollectButtons(Root.ToSharedRef(), Buttons);
				}
			}
			if (!Buttons.IsEmpty())
			{
				ButtonRows.Add(MoveTemp(Buttons));
			}
		}
		return ButtonRows;
	}

	/** The live actor's Details button labelled Label, in the Cambriana category. */
	TSharedPtr<SButton> FindDetailsButton(UObject* Object, const TCHAR* Label)
	{
		for (const TArray<TSharedPtr<SButton>>& Buttons : DetailsButtonRows(Object))
		{
			for (const TSharedPtr<SButton>& Button : Buttons)
			{
				if (LabelOf(Button) == Label)
				{
					Button->SlatePrepass(1.0f);
					return Button;
				}
			}
		}
		return nullptr;
	}

	/** Expected dialog default, from SDD-unreal.md: an actor with a grammar asset bakes SM_<grammar name>_<actor label> into the grammar's bake folder. */
	FString ExpectedActorMeshPackage(const FBakeFixture& Fixture, const ACambrianaLiveActor& Actor)
	{
		return FString(BakeTestRoot) / Fixture.Name / (TEXT("SM_") + Fixture.Name + TEXT("_") + Actor.GetActorLabel());
	}

	FString ObjectPathOf(const FString& PackageName)
	{
		return PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
	}

	/** A live actor placed from the fixture's grammar, its recursion and sides overridden. */
	ACambrianaLiveActor* PlaceOverriddenActor(const FBakeFixture& Fixture)
	{
		ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
		if (Actor)
		{
			Actor->bOverrideRecursion = true;
			Actor->Recursion = OverriddenRecursion;
			Actor->bOverrideSides = true;
			Actor->Sides = OverriddenSides;
			Actor->Rebuild();
		}
		return Actor;
	}

	void RemoveBakedMesh(const FString& PackageName)
	{
		if (UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			UnloadPackage(Package);
		}
		IFileManager::Get().Delete(*PackageFilename(*PackageName));
	}

	/** Answers the Bake location dialog while in scope, recording each default it is shown; Choose gives the answer, empty to cancel. */
	class FAnswerBakeLocation
	{
	public:
		explicit FAnswerBakeLocation(TFunction<FString(const FString&)> Choose)
		{
			Previous = Cambriana::Editor::SetBakeLocationPrompt([this, Choose = MoveTemp(Choose)](const FString& Default)
			{
				Defaults.Add(Default);
				return Choose(Default);
			});
		}

		~FAnswerBakeLocation() { Cambriana::Editor::SetBakeLocationPrompt(MoveTemp(Previous)); }

		FAnswerBakeLocation(const FAnswerBakeLocation&) = delete;
		FAnswerBakeLocation& operator=(const FAnswerBakeLocation&) = delete;

		TArray<FString> Defaults;

	private:
		Cambriana::Editor::FBakeLocationPrompt Previous;
	};

	FString AcceptDefault(const FString& Default)
	{
		return Default;
	}

	FCambrianaBakeResult BakeWithoutSavePrompt(ACambrianaLiveActor& Actor)
	{
		FCambrianaBakeResult Result;
		RunWithoutSavePrompt([&] { Result = Cambriana::Editor::BakeLiveActor(Actor, ECambrianaBakeChoice::StaticMesh); });
		return Result;
	}

	/** Bakes to a skeletal mesh, recording each overwrite question and giving Answer to it. */
	FCambrianaBakeResult BakeSkeletalWithoutSavePrompt(ACambrianaLiveActor& Actor, TArray<FString>& Questions, EAppReturnType::Type Answer = EAppReturnType::No)
	{
		FCambrianaBakeResult Result;
		RunWithoutSavePrompt([&]
		{
			Result = Cambriana::Editor::BakeLiveActor(Actor, ECambrianaBakeChoice::SkeletalMesh,
				[&Questions, Answer](const FText& Question) { Questions.Add(Question.ToString()); return Answer; });
		});
		return Result;
	}

	/**
	 * Expected names, from SDD-unreal.md: the skeletal mesh defaults to SK_<grammar name>_<actor label> in the grammar's bake folder,
	 * and the skeleton and physics asset take the chosen name with its SK_ prefix replaced by SKEL_ and PHYS_.
	 */
	FString ExpectedActorOutputPackage(const FBakeFixture& Fixture, const ACambrianaLiveActor& Actor, const TCHAR* Prefix)
	{
		return FString(BakeTestRoot) / Fixture.Name / (Prefix + Fixture.Name + TEXT("_") + Actor.GetActorLabel());
	}

	/** Removes the packages, in memory and on disk, on construction and when it goes out of scope. */
	class FRemoveBakedPackages
	{
	public:
		explicit FRemoveBakedPackages(TArray<FString> InPackages) : Packages(MoveTemp(InPackages)) { Remove(); }
		~FRemoveBakedPackages() { Remove(); }

		FRemoveBakedPackages(const FRemoveBakedPackages&) = delete;
		FRemoveBakedPackages& operator=(const FRemoveBakedPackages&) = delete;

	private:
		void Remove()
		{
			for (const FString& Package : Packages)
			{
				RemoveBakedMesh(Package);
			}
		}

		TArray<FString> Packages;
	};

	bool PackageExists(const FString& PackageName)
	{
		return FindPackage(nullptr, *PackageName) || IFileManager::Get().FileExists(*PackageFilename(*PackageName));
	}

	int32 RenderedFacesOf(const USkeletalMesh& Mesh)
	{
		const FSkeletalMeshRenderData* RenderData = Mesh.GetResourceForRendering();
		return RenderData && RenderData->LODRenderData.Num() > 0 ? int32(RenderData->LODRenderData[0].GetTotalFaces()) : 0;
	}

	/** The baked skeletal mesh, placed, at its bind pose against the live actor's triangles. */
	FTriangleMatch MatchBindPose(USkeletalMesh& Mesh, ACambrianaLiveActor& Live)
	{
		ASkeletalMeshActor* Placed = PlaceAsset<ASkeletalMeshActor>(&Mesh);
		if (!Placed)
		{
			return FTriangleMatch();
		}
		UDynamicMeshComponent* LiveComponent = Live.GetDynamicMeshComponent();
		const TArray<FMeshTriangle> Skinned = SkinnedTrianglesOf(*Placed->GetSkeletalMeshComponent(), LiveComponent->GetComponentTransform());
		const FTriangleMatch Match = MatchTriangles(*LiveComponent->GetMesh(), Skinned);
		Placed->GetWorld()->EditorDestroyActor(Placed, false);
		return Match;
	}

	// SDD-rigging.md, Physics, by hand: step 10 cm, thickness 20%; the bone `[ + ! F ]` draws, core bone 1 at skeleton bone 2, has
	// length 10 and radius 0.7 (`!` thickness x 0.7), so bending stiffness 0.7^4 / 10, and hangs from the body on skeleton bone 1.
	const TCHAR* const ThicknessGrammarText = TEXT("step 10cm\nthickness 20%\naxiom F [ + ! F ] [ - ' F ] F ! F\n");
	constexpr double SecondBoneBendingStiffness = 0.2401 / 10.0;

	/** The drive stiffness of the constraint on skeleton bone 2's body, or -1 when there is none. */
	double SecondBoneDriveStiffness(const UPhysicsAsset& PhysicsAsset, const USkeletalMesh& Mesh)
	{
		const int32 Index = PhysicsAsset.FindConstraintIndex(Mesh.GetRefSkeleton().GetBoneName(2));
		return PhysicsAsset.ConstraintSetup.IsValidIndex(Index)
			? double(PhysicsAsset.ConstraintSetup[Index]->DefaultInstance.ProfileInstance.AngularDrive.SlerpDrive.Stiffness) : -1.0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDetailsBakeTest,
	"Cambriana.Editor.Bake.LiveActorDetailsBakeWritesAStaticMeshMatchingTheActorWithItsOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDetailsBakeTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBake"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString MeshPackage = ExpectedActorMeshPackage(Fixture, *Actor);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPackage); };

	const TSharedPtr<SButton> Bake = FindDetailsButton(Actor, TEXT("Bake to Static Mesh"));
	if (!TestNotNull(TEXT("The live actor's Details has a Bake to Static Mesh button in its Cambriana category"), Bake.Get()))
	{
		return false;
	}
	FAnswerBakeLocation Answer(&AcceptDefault);
	RunWithoutSavePrompt([&] { Bake->SimulateClick(); });

	TestEqual(TEXT("Bake asks for the location once, defaulting to the grammar's bake folder"), Answer.Defaults, TArray<FString>{ MeshPackage });
	UStaticMesh* Mesh = FindObject<UStaticMesh>(nullptr, *ObjectPathOf(MeshPackage));
	if (!TestNotNull(FString::Printf(TEXT("Bake writes %s"), *MeshPackage), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The static mesh is saved to disk"), IFileManager::Get().FileExists(*PackageFilename(*MeshPackage)) && !Mesh->GetPackage()->IsDirty());
	const FTriangleMatch Match = MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *Mesh);
	TestTrue(FString::Printf(TEXT("The baked mesh's rendered triangles and vertices match the actor's within 0.001 cm (%s)"), *Match.Describe()), Match.Matches());
	TestEqual(TEXT("It has the overridden recursion and sides"), Match.RenderedTriangles, OverriddenTriangles);
	TestNull(TEXT("The grammar's own static mesh is not written"), Fixture.FindMesh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeDialogTest,
	"Cambriana.Editor.Bake.LiveActorBakeLocationDialogIsTheEngineSaveAssetDialogForAStaticMeshWarningBeforeReplacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeDialogTest::RunTest(const FString& Parameters)
{
	const FSaveAssetDialogConfig Config = Cambriana::Editor::MakeBakeLocationDialogConfig(TEXT("/Game/Trees/Fern/SM_Fern_Fern2"), ECambrianaBakeChoice::StaticMesh);
	TestEqual(TEXT("Default folder"), Config.DefaultPath, FString(TEXT("/Game/Trees/Fern")));
	TestEqual(TEXT("Default name"), Config.DefaultAssetName, FString(TEXT("SM_Fern_Fern2")));
	TestTrue(TEXT("Static meshes only"), Config.AssetClassNames == TArray<FTopLevelAssetPath>{ UStaticMesh::StaticClass()->GetClassPathName() });
	// Under this policy SAssetDialog asks "<name> already exists. Do you want to replace it?" for an existing asset of the class.
	TestEqual(TEXT("An existing static mesh may be chosen after the dialog's own warning"), int32(Config.ExistingAssetPolicy),
		int32(ESaveAssetDialogExistingAssetPolicy::AllowButWarn));
	TestFalse(TEXT("The dialog has a title"), Config.DialogTitleOverride.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeRebakeTest,
	"Cambriana.Editor.Bake.LiveActorBakeRebuildsTheChosenStaticMeshInPlaceWithoutASecondQuestionAndCancelBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeRebakeTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeRebake"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString MeshPackage = ExpectedActorMeshPackage(Fixture, *Actor);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPackage); };

	FCambrianaBakeResult First;
	{
		FAnswerBakeLocation Answer(&AcceptDefault);
		First = BakeWithoutSavePrompt(*Actor);
	}
	if (!TestTrue(TEXT("The first bake succeeds"), First.Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}

	Actor->Sides = 7;
	Actor->Rebuild();
	{
		FAnswerBakeLocation Cancel([](const FString&) { return FString(); });
		const FCambrianaBakeResult Cancelled = BakeWithoutSavePrompt(*Actor);
		TestTrue(TEXT("Cancelling the location dialog bakes nothing"), Cancelled.Status == ECambrianaBakeStatus::LocationNotChosen);
		TestEqual(TEXT("The dialog was shown"), Cancel.Defaults.Num(), 1);
	}
	TestEqual(TEXT("The static mesh keeps its triangles"), RenderedTrianglesOf(*First.StaticMesh).Num(), OverriddenTriangles);

	// The dialog has already confirmed replacing the existing asset; unattended, any further question would answer No.
	FAnswerBakeLocation Answer(&AcceptDefault);
	const FCambrianaBakeResult Rebaked = BakeWithoutSavePrompt(*Actor);
	TestTrue(TEXT("Choosing the existing static mesh bakes without asking again"), Rebaked.Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("The same static mesh object is rebuilt, so references to it hold"), Rebaked.StaticMesh == First.StaticMesh);
	TestTrue(TEXT("It matches the actor's new sides"), MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *First.StaticMesh).Matches());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeChosenLocationTest,
	"Cambriana.Editor.Bake.LiveActorBakeWritesWhereTheDialogSays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeChosenLocationTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeChosen"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString Chosen = FString(BakeTestRoot) / TEXT("ActorBakeChosen") / TEXT("SM_Chosen");
	ON_SCOPE_EXIT { RemoveBakedMesh(Chosen); };

	FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });
	const FCambrianaBakeResult Result = BakeWithoutSavePrompt(*Actor);

	TestTrue(TEXT("The bake succeeds"), Result.Status == ECambrianaBakeStatus::Baked);
	UStaticMesh* Mesh = FindObject<UStaticMesh>(nullptr, *ObjectPathOf(Chosen));
	TestTrue(TEXT("The static mesh is at the chosen location"), Mesh && Result.StaticMesh == Mesh);
	TestNull(TEXT("Nothing is written at the default location"), FindObject<UStaticMesh>(nullptr, *ObjectPathOf(ExpectedActorMeshPackage(Fixture, *Actor))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeReplaceYesTest,
	"Cambriana.Editor.Bake.LiveActorBakeReplacesAnAssetOfAnotherClassAtTheChosenLocationOnYes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeReplaceYesTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeReplaceYes"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString Chosen = FString(BakeTestRoot) / TEXT("ActorBakeReplaceYes") / TEXT("SM_Chosen");
	ON_SCOPE_EXIT { RemoveBakedMesh(Chosen); };
	const TWeakObjectPtr<UObject> Occupant = NewObject<UCambrianaGrammar>(CreatePackage(*Chosen), TEXT("SM_Chosen"), RF_Public | RF_Standalone);

	FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });
	TArray<FString> Questions;
	FCambrianaBakeResult Result;
	RunWithoutSavePrompt([&]
	{
		Result = Cambriana::Editor::BakeLiveActor(*Actor, ECambrianaBakeChoice::StaticMesh, nullptr, [&Questions](const FText& Question) { Questions.Add(Question.ToString()); return EAppReturnType::Yes; });
	});

	TestTrue(TEXT("The bake succeeds"), Result.Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("Replacing is asked once, naming the asset"),
		Questions.Num() == 1 && Questions[0].StartsWith(TEXT("Do you want to replace the existing asset?")) && Questions[0].EndsWith(Chosen));
	TestFalse(TEXT("The other asset is deleted"), Occupant.IsValid());
	TestTrue(TEXT("A static mesh is at the chosen location"), Result.StaticMesh && FindObject<UStaticMesh>(nullptr, *ObjectPathOf(Chosen)) == Result.StaticMesh);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeReplaceNoTest,
	"Cambriana.Editor.Bake.LiveActorBakeLeavesAnAssetOfAnotherClassAtTheChosenLocationOnNoAndBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeReplaceNoTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeReplaceNo"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString Chosen = FString(BakeTestRoot) / TEXT("ActorBakeReplaceNo") / TEXT("SM_Chosen");
	ON_SCOPE_EXIT { RemoveBakedMesh(Chosen); };
	UCambrianaGrammar* Occupant = NewObject<UCambrianaGrammar>(CreatePackage(*Chosen), TEXT("SM_Chosen"), RF_Public | RF_Standalone);

	FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });
	int32 Asked = 0;
	FCambrianaBakeResult Result;
	RunWithoutSavePrompt([&]
	{
		Result = Cambriana::Editor::BakeLiveActor(*Actor, ECambrianaBakeChoice::StaticMesh, nullptr, [&Asked](const FText&) { ++Asked; return EAppReturnType::No; });
	});

	TestTrue(TEXT("The bake reports the overwrite declined"), Result.Status == ECambrianaBakeStatus::OverwriteDeclined);
	TestEqual(TEXT("Replacing is asked once"), Asked, 1);
	TestTrue(TEXT("The other asset is untouched"), FindObject<UObject>(nullptr, *ObjectPathOf(Chosen)) == Occupant && IsValid(Occupant));
	TestNull(TEXT("No static mesh is created"), Result.StaticMesh);
	TestFalse(TEXT("Nothing is saved"), IFileManager::Get().FileExists(*PackageFilename(*Chosen)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaInlineLiveActorBakeTest,
	"Cambriana.Editor.Bake.InlineLiveActorBakeDefaultsToTheContentRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaInlineLiveActorBakeTest::RunTest(const FString& Parameters)
{
	ACambrianaLiveActor* Actor = SpawnInlineActor(*LoadFile(FixturePath(TEXT("sk.ls"))));
	if (!TestNotNull(TEXT("A live actor with inline text"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	Actor->SetActorLabel(TEXT("InlineBakeTestActor"));
	Actor->Rebuild();
	// Expected dialog default, from SDD-unreal.md: without a grammar asset, SM_<actor label> at the project's content root.
	const FString MeshPackage = TEXT("/Game/SM_InlineBakeTestActor");
	RemoveBakedMesh(MeshPackage);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPackage); };

	FAnswerBakeLocation Answer(&AcceptDefault);
	const FCambrianaBakeResult Result = BakeWithoutSavePrompt(*Actor);

	TestEqual(TEXT("The dialog defaults to the content root"), Answer.Defaults, TArray<FString>{ MeshPackage });
	TestTrue(TEXT("The bake succeeds"), Result.Status == ECambrianaBakeStatus::Baked);
	UStaticMesh* Mesh = FindObject<UStaticMesh>(nullptr, *ObjectPathOf(MeshPackage));
	if (!TestNotNull(FString::Printf(TEXT("Bake writes %s"), *MeshPackage), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The result is that mesh"), Result.StaticMesh == Mesh);
	TestTrue(TEXT("It matches the actor"), MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *Mesh).Matches());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeUnsavedGrammarTest,
	"Cambriana.Editor.Bake.LiveActorBakeWithANeverSavedGrammarBakesWithoutSavingIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeUnsavedGrammarTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeUnsaved"));
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString MeshPackage = ExpectedActorMeshPackage(Fixture, *Actor);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPackage); };
	FAnswerBakeLocation Answer(&AcceptDefault);

	const FCambrianaBakeResult Result = BakeWithoutSavePrompt(*Actor);

	TestTrue(TEXT("The bake succeeds"), Result.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("The location is asked for once"), Answer.Defaults.Num(), 1);
	UStaticMesh* Mesh = FindObject<UStaticMesh>(nullptr, *ObjectPathOf(MeshPackage));
	TestTrue(TEXT("It matches the actor"), Mesh && MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *Mesh).Matches());
	TestFalse(TEXT("The never-saved grammar is not saved"), IFileManager::Get().FileExists(*PackageFilename(*Fixture.GrammarPackage)));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDetailsBakeButtonsTest,
	"Cambriana.Editor.Bake.LiveActorDetailsOffersBakeToStaticMeshThenBakeToSkeletalMeshOnOneRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDetailsBakeButtonsTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeButtons"));
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };

	const TArray<TArray<TSharedPtr<SButton>>> Rows = DetailsButtonRows(Actor);
	const TArray<TSharedPtr<SButton>>* BakeRow = Rows.FindByPredicate([](const TArray<TSharedPtr<SButton>>& Buttons)
	{
		return Buttons.ContainsByPredicate([](const TSharedPtr<SButton>& Button) { return LabelOf(Button).StartsWith(TEXT("Bake")); });
	});
	if (!TestNotNull(TEXT("The Cambriana category has a row of Bake buttons"), BakeRow))
	{
		return false;
	}
	TArray<FString> Labels;
	for (const TSharedPtr<SButton>& Button : *BakeRow)
	{
		Labels.Add(LabelOf(Button));
	}
	TestEqual(TEXT("The row holds Bake to Static Mesh and Bake to Skeletal Mesh, in that order"), Labels,
		TArray<FString>{ TEXT("Bake to Static Mesh"), TEXT("Bake to Skeletal Mesh") });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDetailsSkeletalBakeTest,
	"Cambriana.Editor.Bake.LiveActorDetailsBakeToSkeletalMeshWritesASkeletalMeshSkeletonAndPhysicsAssetMatchingTheActorWithItsOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDetailsSkeletalBakeTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorSkeletalBake"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString MeshPackage = ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SK_"));
	const FString SkeletonPackage = ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SKEL_"));
	const FString PhysicsPackage = ExpectedActorOutputPackage(Fixture, *Actor, TEXT("PHYS_"));
	FRemoveBakedPackages Cleanup({ MeshPackage, SkeletonPackage, PhysicsPackage });

	const TSharedPtr<SButton> Bake = FindDetailsButton(Actor, TEXT("Bake to Skeletal Mesh"));
	if (!TestNotNull(TEXT("The live actor's Details has a Bake to Skeletal Mesh button in its Cambriana category"), Bake.Get()))
	{
		return false;
	}
	FAnswerBakeLocation Answer(&AcceptDefault);
	RunWithoutSavePrompt([&] { Bake->SimulateClick(); });

	TestEqual(TEXT("Bake asks for the location once, defaulting to SK_ in the grammar's bake folder"), Answer.Defaults, TArray<FString>{ MeshPackage });
	USkeletalMesh* Mesh = FindObject<USkeletalMesh>(nullptr, *ObjectPathOf(MeshPackage));
	USkeleton* Skeleton = FindObject<USkeleton>(nullptr, *ObjectPathOf(SkeletonPackage));
	UPhysicsAsset* PhysicsAsset = FindObject<UPhysicsAsset>(nullptr, *ObjectPathOf(PhysicsPackage));
	if (!TestNotNull(FString::Printf(TEXT("Bake writes %s"), *MeshPackage), Mesh)
		|| !TestNotNull(FString::Printf(TEXT("Bake writes %s"), *SkeletonPackage), Skeleton)
		|| !TestNotNull(FString::Printf(TEXT("Bake writes %s"), *PhysicsPackage), PhysicsAsset))
	{
		return false;
	}
	for (const UObject* Written : { static_cast<const UObject*>(Mesh), static_cast<const UObject*>(Skeleton), static_cast<const UObject*>(PhysicsAsset) })
	{
		TestTrue(FString::Printf(TEXT("%s is saved to disk"), *Written->GetName()),
			IFileManager::Get().FileExists(*PackageFilename(*Written->GetPackage()->GetName())) && !Written->GetPackage()->IsDirty());
	}
	TestTrue(TEXT("The skeletal mesh uses the skeleton"), Mesh->GetSkeleton() == Skeleton);
	TestTrue(TEXT("The skeletal mesh uses the physics asset"), Mesh->GetPhysicsAsset() == PhysicsAsset);

	const FTriangleMatch Match = MatchBindPose(*Mesh, *Actor);
	TestTrue(FString::Printf(TEXT("The baked mesh at bind pose matches the actor's triangles and vertices within 0.001 cm (%s)"), *Match.Describe()), Match.Matches());
	TestEqual(TEXT("It has the overridden recursion and sides"), Match.RenderedTriangles, OverriddenTriangles);

	TestEqual(TEXT("The skeleton has one bone per drawn bone plus Root"), Skeleton->GetReferenceSkeleton().GetRawBoneNum(), OverriddenBones + 1);
	TestEqual(TEXT("That is one per bone of the actor's build plus Root"), Skeleton->GetReferenceSkeleton().GetRawBoneNum(),
		Cambriana::BuildMesh(Actor->MakeBuildInput()).Bones.Num() + 1);
	TestEqual(TEXT("Its first bone is Root"), Skeleton->GetReferenceSkeleton().GetBoneName(0), FName(TEXT("Root")));
	TestEqual(TEXT("The physics asset has one body per drawn bone"), PhysicsAsset->SkeletalBodySetups.Num(), OverriddenBones);
	TestNull(TEXT("The skeletal mesh records no grammar"), UCambrianaAssetImportData::Of(Mesh));
	TestNull(TEXT("The grammar's own skeletal mesh is not written"), Fixture.FindSkeletalMesh());
	TestNull(TEXT("The grammar's own skeleton is not written"), Fixture.FindSkeleton());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalBakeDialogTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshLocationDialogIsTheEngineSaveAssetDialogForASkeletalMeshWarningBeforeReplacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalBakeDialogTest::RunTest(const FString& Parameters)
{
	const FSaveAssetDialogConfig Config = Cambriana::Editor::MakeBakeLocationDialogConfig(TEXT("/Game/Trees/Fern/SK_Fern_Fern2"), ECambrianaBakeChoice::SkeletalMesh);
	TestEqual(TEXT("Default folder"), Config.DefaultPath, FString(TEXT("/Game/Trees/Fern")));
	TestEqual(TEXT("Default name"), Config.DefaultAssetName, FString(TEXT("SK_Fern_Fern2")));
	TestTrue(TEXT("Skeletal meshes only"), Config.AssetClassNames == TArray<FTopLevelAssetPath>{ USkeletalMesh::StaticClass()->GetClassPathName() });
	TestEqual(TEXT("An existing skeletal mesh may be chosen after the dialog's own warning"), int32(Config.ExistingAssetPolicy),
		int32(ESaveAssetDialogExistingAssetPolicy::AllowButWarn));
	const FSaveAssetDialogConfig StaticConfig = Cambriana::Editor::MakeBakeLocationDialogConfig(TEXT("/Game/Trees/Fern/SM_Fern_Fern2"), ECambrianaBakeChoice::StaticMesh);
	TestFalse(TEXT("The dialog has a title"), Config.DialogTitleOverride.IsEmpty());
	TestFalse(TEXT("Its title differs from the static mesh dialog's"), Config.DialogTitleOverride.EqualTo(StaticConfig.DialogTitleOverride));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalBakeNamesTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshNamesTheSkeletonAndPhysicsAssetFromTheChosenName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalBakeNamesTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorSkeletalNames"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString Folder = FString(BakeTestRoot) / TEXT("ActorSkeletalNames");
	FRemoveBakedPackages Cleanup({ Folder / TEXT("SK_Chosen"), Folder / TEXT("SKEL_Chosen"), Folder / TEXT("PHYS_Chosen"),
		Folder / TEXT("Tree"), Folder / TEXT("SKEL_Tree"), Folder / TEXT("PHYS_Tree") });

	struct FCase
	{
		const TCHAR* Chosen;
		const TCHAR* Skeleton;
		const TCHAR* PhysicsAsset;
	};
	for (const FCase& Case : { FCase{ TEXT("SK_Chosen"), TEXT("SKEL_Chosen"), TEXT("PHYS_Chosen") }, FCase{ TEXT("Tree"), TEXT("SKEL_Tree"), TEXT("PHYS_Tree") } })
	{
		const FString Chosen = Folder / Case.Chosen;
		FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });
		TArray<FString> Questions;
		const FCambrianaBakeResult Result = BakeSkeletalWithoutSavePrompt(*Actor, Questions);

		TestTrue(FString::Printf(TEXT("%s: the bake succeeds"), Case.Chosen), Result.Status == ECambrianaBakeStatus::Baked);
		TestEqual(FString::Printf(TEXT("%s: nothing is asked for new outputs"), Case.Chosen), Questions.Num(), 0);
		USkeletalMesh* Mesh = FindObject<USkeletalMesh>(nullptr, *ObjectPathOf(Chosen));
		TestTrue(FString::Printf(TEXT("%s: the skeletal mesh is at the chosen location"), Case.Chosen), Mesh && Result.SkeletalMesh == Mesh);
		USkeleton* Skeleton = FindObject<USkeleton>(nullptr, *ObjectPathOf(Folder / Case.Skeleton));
		UPhysicsAsset* PhysicsAsset = FindObject<UPhysicsAsset>(nullptr, *ObjectPathOf(Folder / Case.PhysicsAsset));
		TestTrue(FString::Printf(TEXT("%s: the skeleton is %s beside it"), Case.Chosen, Case.Skeleton), Skeleton && Result.Skeleton == Skeleton);
		TestTrue(FString::Printf(TEXT("%s: the physics asset is %s beside it"), Case.Chosen, Case.PhysicsAsset), PhysicsAsset && Result.PhysicsAsset == PhysicsAsset);
	}
	TestFalse(TEXT("Nothing is written at the default location"), PackageExists(ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SK_"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalRebakeTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshAsksOnceBeforeRebuildingTheSkeletonAndPhysicsAssetBesideTheChosenMeshInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalRebakeTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorSkeletalRebake"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString Folder = FString(BakeTestRoot) / TEXT("ActorSkeletalRebake");
	FRemoveBakedPackages Cleanup({ Folder / TEXT("SK_Chosen"), Folder / TEXT("SKEL_Chosen"), Folder / TEXT("PHYS_Chosen") });
	const FString Chosen = Folder / TEXT("SK_Chosen");
	FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });

	TArray<FString> Questions;
	const FCambrianaBakeResult First = BakeSkeletalWithoutSavePrompt(*Actor, Questions);
	if (!TestTrue(TEXT("The first bake succeeds"), First.Status == ECambrianaBakeStatus::Baked && First.SkeletalMesh && First.Skeleton && First.PhysicsAsset))
	{
		return false;
	}

	Actor->Sides = 7;
	Actor->Rebuild();
	Questions.Reset();
	const FCambrianaBakeResult Declined = BakeSkeletalWithoutSavePrompt(*Actor, Questions, EAppReturnType::No);
	TestTrue(TEXT("No bakes nothing"), Declined.Status == ECambrianaBakeStatus::OverwriteDeclined);
	TestTrue(TEXT("The overwrite question is asked once, naming the skeleton"), Questions.Num() == 1
		&& Questions[0].StartsWith(TEXT("Do you want to overwrite the existing asset?")) && Questions[0].EndsWith(Folder / TEXT("SKEL_Chosen")));
	TestEqual(TEXT("The skeletal mesh keeps its triangles"), RenderedFacesOf(*First.SkeletalMesh), OverriddenTriangles);

	Questions.Reset();
	const FCambrianaBakeResult Rebaked = BakeSkeletalWithoutSavePrompt(*Actor, Questions, EAppReturnType::Yes);
	TestTrue(TEXT("Yes bakes"), Rebaked.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("It asks once"), Questions.Num(), 1);
	TestTrue(TEXT("The same skeletal mesh, skeleton and physics asset objects are rebuilt, so references to them hold"),
		Rebaked.SkeletalMesh == First.SkeletalMesh && Rebaked.Skeleton == First.Skeleton && Rebaked.PhysicsAsset == First.PhysicsAsset);
	// 4 branches * 6 segments * 2 triangles per side * 7 sides.
	TestEqual(TEXT("It has the actor's new sides"), RenderedFacesOf(*First.SkeletalMesh), 4 * 6 * 2 * 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalExistingBesideNewTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshAsksBeforeAnExistingSkeletonOrPhysicsAssetBesideANewSkeletalMeshAndBakesNothingOnNo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalExistingBesideNewTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorSkeletalExisting"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString Folder = FString(BakeTestRoot) / TEXT("ActorSkeletalExisting");
	FRemoveBakedPackages Cleanup({ Folder / TEXT("SK_A"), Folder / TEXT("SKEL_A"), Folder / TEXT("PHYS_A"),
		Folder / TEXT("SK_B"), Folder / TEXT("SKEL_B"), Folder / TEXT("PHYS_B") });
	NewObject<USkeleton>(CreatePackage(*(Folder / TEXT("SKEL_A"))), TEXT("SKEL_A"), RF_Public | RF_Standalone);
	NewObject<UPhysicsAsset>(CreatePackage(*(Folder / TEXT("PHYS_B"))), TEXT("PHYS_B"), RF_Public | RF_Standalone);

	struct FCase
	{
		const TCHAR* Chosen;
		const TCHAR* Existing;
		const TCHAR* NotWritten;
	};
	for (const FCase& Case : { FCase{ TEXT("SK_A"), TEXT("SKEL_A"), TEXT("PHYS_A") }, FCase{ TEXT("SK_B"), TEXT("PHYS_B"), TEXT("SKEL_B") } })
	{
		const FString Chosen = Folder / Case.Chosen;
		FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });
		TArray<FString> Questions;
		const FCambrianaBakeResult Result = BakeSkeletalWithoutSavePrompt(*Actor, Questions, EAppReturnType::No);

		TestTrue(FString::Printf(TEXT("%s: No bakes nothing"), Case.Chosen), Result.Status == ECambrianaBakeStatus::OverwriteDeclined);
		TestTrue(FString::Printf(TEXT("%s: the overwrite question is asked once, naming %s"), Case.Chosen, Case.Existing),
			Questions.Num() == 1 && Questions[0].StartsWith(TEXT("Do you want to overwrite the existing asset?")) && Questions[0].EndsWith(Folder / Case.Existing));
		TestFalse(FString::Printf(TEXT("%s: no skeletal mesh is written"), Case.Chosen), PackageExists(Chosen));
		TestFalse(FString::Printf(TEXT("%s: %s is not written"), Case.Chosen, Case.NotWritten), PackageExists(Folder / Case.NotWritten));
		TestFalse(FString::Printf(TEXT("%s: %s is not saved"), Case.Chosen, Case.Existing), IFileManager::Get().FileExists(*PackageFilename(*(Folder / Case.Existing))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalCancelTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshCancelledInTheDialogWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalCancelTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorSkeletalCancel"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const TArray<FString> Outputs = { ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SK_")), ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SKEL_")),
		ExpectedActorOutputPackage(Fixture, *Actor, TEXT("PHYS_")) };
	FRemoveBakedPackages Cleanup(Outputs);

	FAnswerBakeLocation Cancel([](const FString&) { return FString(); });
	TArray<FString> Questions;
	const FCambrianaBakeResult Result = BakeSkeletalWithoutSavePrompt(*Actor, Questions);

	TestTrue(TEXT("Cancelling the location dialog bakes nothing"), Result.Status == ECambrianaBakeStatus::LocationNotChosen);
	TestEqual(TEXT("The dialog was shown once, defaulting to SK_ in the grammar's bake folder"), Cancel.Defaults, TArray<FString>{ Outputs[0] });
	TestEqual(TEXT("Nothing else is asked"), Questions.Num(), 0);
	for (const FString& Output : Outputs)
	{
		TestFalse(FString::Printf(TEXT("%s is not written"), *Output), PackageExists(Output));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalUnlinksTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshOverAGrammarsSkeletalMeshRebuildsItInPlaceWithoutItsGrammarLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalUnlinksTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorSkeletalUnlinks"));
	if (!TestTrue(TEXT("The grammar bakes to a skeletal mesh"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	USkeletalMesh* Linked = Fixture.FindSkeletalMesh();
	if (!TestNotNull(TEXT("The grammar's skeletal mesh"), Linked) || !TestNotNull(TEXT("It is linked to the grammar"), UCambrianaAssetImportData::Of(Linked)))
	{
		return false;
	}
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };

	const FString Chosen = Fixture.SkeletalMeshPackage;
	FAnswerBakeLocation Answer([Chosen](const FString&) { return Chosen; });
	TArray<FString> Questions;
	const FCambrianaBakeResult Result = BakeSkeletalWithoutSavePrompt(*Actor, Questions, EAppReturnType::Yes);

	TestTrue(TEXT("The bake succeeds"), Result.Status == ECambrianaBakeStatus::Baked);
	TestTrue(TEXT("It asks once, naming the grammar's skeleton beside the chosen mesh"), Questions.Num() == 1 && Questions[0].EndsWith(Fixture.SkeletonPackage));
	TestTrue(TEXT("The grammar's skeletal mesh object is rebuilt"), Result.SkeletalMesh == Linked);
	TestTrue(TEXT("Its skeleton and physics asset are the grammar's"), Result.Skeleton == Fixture.FindSkeleton() && Result.PhysicsAsset == Fixture.FindPhysicsAsset());
	TestEqual(TEXT("It has the actor's overridden recursion and sides"), RenderedFacesOf(*Linked), OverriddenTriangles);
	TestNull(TEXT("It no longer records the grammar"), UCambrianaAssetImportData::Of(Linked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkeletalStiffnessTest,
	"Cambriana.Editor.Bake.LiveActorBakeToSkeletalMeshTakesPhysicsStiffnessFromTheGrammarAssetOrTheDefaultForInlineText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkeletalStiffnessTest::RunTest(const FString& Parameters)
{
	{
		FBakeFixture Fixture(TEXT("ActorSkeletalStiffness"));
		Fixture.Grammar->GrammarText = ThicknessGrammarText;
		Fixture.Grammar->Bake.PhysicsStiffness = 1000.0f;
		ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
		if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
		{
			return false;
		}
		ON_SCOPE_EXIT { DestroyActor(Actor); };
		FRemoveBakedPackages Cleanup({ ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SK_")), ExpectedActorOutputPackage(Fixture, *Actor, TEXT("SKEL_")),
			ExpectedActorOutputPackage(Fixture, *Actor, TEXT("PHYS_")) });
		FAnswerBakeLocation Answer(&AcceptDefault);
		TArray<FString> Questions;
		const FCambrianaBakeResult Result = BakeSkeletalWithoutSavePrompt(*Actor, Questions);
		if (TestTrue(TEXT("The actor with a grammar asset bakes"), Result.Status == ECambrianaBakeStatus::Baked && Result.PhysicsAsset && Result.SkeletalMesh))
		{
			const double Expected = 1000.0 * SecondBoneBendingStiffness;
			TestEqual(TEXT("Its drive stiffness is the grammar asset's Physics Stiffness times the bone's bending stiffness"),
				SecondBoneDriveStiffness(*Result.PhysicsAsset, *Result.SkeletalMesh), Expected, Expected * 1e-5);
		}
	}

	ACambrianaLiveActor* Inline = SpawnInlineActor(ThicknessGrammarText);
	if (!TestNotNull(TEXT("A live actor with inline text"), Inline))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Inline); };
	Inline->SetActorLabel(TEXT("InlineSkeletalBakeTestActor"));
	Inline->Rebuild();
	// Expected names, from SDD-unreal.md: without a grammar asset, SK_<actor label> at the project's content root, the skeleton and
	// physics asset beside it.
	const FString MeshPackage = TEXT("/Game/SK_InlineSkeletalBakeTestActor");
	FRemoveBakedPackages Cleanup({ MeshPackage, TEXT("/Game/SKEL_InlineSkeletalBakeTestActor"), TEXT("/Game/PHYS_InlineSkeletalBakeTestActor") });
	FAnswerBakeLocation Answer(&AcceptDefault);
	TArray<FString> Questions;
	const FCambrianaBakeResult Result = BakeSkeletalWithoutSavePrompt(*Inline, Questions);

	TestEqual(TEXT("The dialog defaults to the content root"), Answer.Defaults, TArray<FString>{ MeshPackage });
	if (!TestTrue(TEXT("The inline actor bakes"), Result.Status == ECambrianaBakeStatus::Baked && Result.PhysicsAsset && Result.SkeletalMesh && Result.Skeleton))
	{
		return false;
	}
	TestEqual(TEXT("The skeleton is beside it"), Result.Skeleton->GetPathName(), FString(TEXT("/Game/SKEL_InlineSkeletalBakeTestActor.SKEL_InlineSkeletalBakeTestActor")));
	TestEqual(TEXT("The physics asset is beside it"), Result.PhysicsAsset->GetPathName(), FString(TEXT("/Game/PHYS_InlineSkeletalBakeTestActor.PHYS_InlineSkeletalBakeTestActor")));
	// SDD-rigging.md, Physics: PhysicsStiffness is 25000 by default.
	const double Expected = 25000.0 * SecondBoneBendingStiffness;
	TestEqual(TEXT("Its drive stiffness is the default Physics Stiffness times the bone's bending stiffness"),
		SecondBoneDriveStiffness(*Result.PhysicsAsset, *Result.SkeletalMesh), Expected, Expected * 1e-5);
	return true;
}

#endif
