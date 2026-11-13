#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "Components/DynamicMeshComponent.h"
#include "IContentBrowserSingleton.h"
#include "IDetailTreeNode.h"
#include "Misc/AutomationTest.h"
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

	/** The live actor's Details button labelled Label, in the Cambriana category. */
	TSharedPtr<SButton> FindDetailsButton(UObject* Object, const TCHAR* Label)
	{
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Object });
		const TSharedPtr<IDetailTreeNode> Category = FindCategory(*Rows, TEXT("Cambriana"));
		if (!Category)
		{
			return nullptr;
		}
		TArray<TSharedRef<IDetailTreeNode>> Nodes;
		Category->GetChildren(Nodes, true);
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const FNodeWidgets Widgets = Node->CreateNodeWidgets();
			for (const TSharedPtr<SWidget>& Root : { Widgets.NameWidget, Widgets.ValueWidget, Widgets.WholeRowWidget })
			{
				const TSharedPtr<SWidget> Button = Root ? FindWidgetOfType(Root.ToSharedRef(), TEXT("SButton")) : nullptr;
				const TSharedPtr<SWidget> Text = Button ? FindWidgetOfType(Button.ToSharedRef(), TEXT("STextBlock")) : nullptr;
				if (Text && StaticCastSharedPtr<STextBlock>(Text)->GetText().ToString() == Label)
				{
					Button->SlatePrepass(1.0f);
					return StaticCastSharedPtr<SButton>(Button);
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
		RunWithoutSavePrompt([&] { Result = Cambriana::Editor::BakeLiveActor(Actor); });
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDetailsBakeTest,
	"Cambriana.Editor.Bake.LiveActorDetailsBakeWritesAStaticMeshMatchingTheActorWithItsOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDetailsBakeTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBake"));
	// The grammar's own bake outputs do not apply to the actor's Details Bake, which writes a static mesh only.
	Fixture.Grammar->Bake.bStaticMesh = false;
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString MeshPackage = ExpectedActorMeshPackage(Fixture, *Actor);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPackage); };

	const TSharedPtr<SButton> Bake = FindDetailsButton(Actor, TEXT("Bake"));
	if (!TestNotNull(TEXT("The live actor's Details has a Bake button in its Cambriana category"), Bake.Get()))
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
	const FSaveAssetDialogConfig Config = Cambriana::Editor::MakeBakeLocationDialogConfig(TEXT("/Game/Trees/Fern/SM_Fern_Fern2"));
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
	"Cambriana.Editor.Bake.LiveActorBakeWithAGrammarWhoseSaveIsCancelledBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeUnsavedGrammarTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("the grammar is not saved; nothing is baked"), ELogVerbosity::Warning);
	FBakeFixture Fixture(TEXT("ActorBakeUnsaved"));
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	FAnswerBakeLocation Answer(&AcceptDefault);

	// With -unattended the engine's save prompt cancels.
	const FCambrianaBakeResult Result = Cambriana::Editor::BakeLiveActor(*Actor);

	TestTrue(TEXT("The bake reports the grammar not saved"), Result.Status == ECambrianaBakeStatus::GrammarNotSaved);
	TestEqual(TEXT("The location is not asked for"), Answer.Defaults.Num(), 0);
	TestNull(TEXT("No static mesh is created"), FindObject<UStaticMesh>(nullptr, *ObjectPathOf(ExpectedActorMeshPackage(Fixture, *Actor))));
	return true;
}

#endif
