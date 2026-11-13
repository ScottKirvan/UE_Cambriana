#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "Components/DynamicMeshComponent.h"
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

	/** Expected location, from SDD-unreal.md: an actor with a grammar asset bakes SM_<grammar name>_<actor label> into the grammar's bake folder. */
	FString ExpectedActorMeshPath(const FBakeFixture& Fixture, const ACambrianaLiveActor& Actor)
	{
		const FString Name = TEXT("SM_") + Fixture.Name + TEXT("_") + Actor.GetActorLabel();
		return FString(BakeTestRoot) / Fixture.Name / Name + TEXT(".") + Name;
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

	void RemoveBakedMesh(const FString& ObjectPath)
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
		if (UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			UnloadPackage(Package);
		}
		IFileManager::Get().Delete(*PackageFilename(*PackageName));
	}

	Cambriana::Editor::FAskToOverwrite CountAnswers(EAppReturnType::Type Reply, int32& Asked)
	{
		return [Reply, &Asked](const FText&) { ++Asked; return Reply; };
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
	const FString MeshPath = ExpectedActorMeshPath(Fixture, *Actor);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPath); };

	const TSharedPtr<SButton> Bake = FindDetailsButton(Actor, TEXT("Bake"));
	if (!TestNotNull(TEXT("The live actor's Details has a Bake button in its Cambriana category"), Bake.Get()))
	{
		return false;
	}
	RunWithoutSavePrompt([&] { Bake->SimulateClick(); });

	UStaticMesh* Mesh = FindObject<UStaticMesh>(nullptr, *MeshPath);
	if (!TestNotNull(FString::Printf(TEXT("Bake writes %s"), *MeshPath), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The static mesh is saved to disk"),
		IFileManager::Get().FileExists(*PackageFilename(*FPackageName::ObjectPathToPackageName(MeshPath))) && !Mesh->GetPackage()->IsDirty());
	const FTriangleMatch Match = MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *Mesh);
	TestTrue(FString::Printf(TEXT("The baked mesh's rendered triangles and vertices match the actor's within 0.001 cm (%s)"), *Match.Describe()), Match.Matches());
	TestEqual(TEXT("It has the overridden recursion and sides"), Match.RenderedTriangles, OverriddenTriangles);
	TestNull(TEXT("The grammar's own static mesh is not written"), Fixture.FindMesh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeOverwriteTest,
	"Cambriana.Editor.Bake.LiveActorBakeAsksBeforeRebuildingItsStaticMeshInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeOverwriteTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ActorBakeOverwrite"));
	ACambrianaLiveActor* Actor = PlaceOverriddenActor(Fixture);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	const FString MeshPath = ExpectedActorMeshPath(Fixture, *Actor);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPath); };

	int32 Asked = 0;
	FCambrianaBakeResult First;
	RunWithoutSavePrompt([&] { First = Cambriana::Editor::BakeLiveActor(*Actor, CountAnswers(EAppReturnType::Yes, Asked)); });
	if (!TestTrue(TEXT("The first bake succeeds"), First.Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	TestEqual(TEXT("A first bake does not ask to overwrite"), Asked, 0);

	Actor->Sides = 7;
	Actor->Rebuild();
	FCambrianaBakeResult Declined;
	RunWithoutSavePrompt([&] { Declined = Cambriana::Editor::BakeLiveActor(*Actor, CountAnswers(EAppReturnType::No, Asked)); });
	TestTrue(TEXT("Declining the overwrite bakes nothing"), Declined.Status == ECambrianaBakeStatus::OverwriteDeclined);
	TestEqual(TEXT("The static mesh keeps its triangles"), RenderedTrianglesOf(*First.StaticMesh).Num(), OverriddenTriangles);

	FCambrianaBakeResult Accepted;
	RunWithoutSavePrompt([&] { Accepted = Cambriana::Editor::BakeLiveActor(*Actor, CountAnswers(EAppReturnType::Yes, Asked)); });
	TestTrue(TEXT("Accepting the overwrite bakes"), Accepted.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("Overwriting is asked once per later bake"), Asked, 2);
	TestTrue(TEXT("The same static mesh object is rebuilt, so references to it hold"), Accepted.StaticMesh == First.StaticMesh);
	TestTrue(TEXT("It matches the actor's new sides"), MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *Accepted.StaticMesh).Matches());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaInlineLiveActorBakeTest,
	"Cambriana.Editor.Bake.InlineLiveActorBakeWritesItsStaticMeshAtTheContentRoot",
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
	// Expected location, from SDD-unreal.md: without a grammar asset, SM_<actor label> at the project's content root.
	const FString MeshPath = TEXT("/Game/SM_InlineBakeTestActor.SM_InlineBakeTestActor");
	RemoveBakedMesh(MeshPath);
	ON_SCOPE_EXIT { RemoveBakedMesh(MeshPath); };

	FCambrianaBakeResult Result;
	RunWithoutSavePrompt([&] { Result = Cambriana::Editor::BakeLiveActor(*Actor); });

	TestTrue(TEXT("The bake succeeds"), Result.Status == ECambrianaBakeStatus::Baked);
	UStaticMesh* Mesh = FindObject<UStaticMesh>(nullptr, *MeshPath);
	if (!TestNotNull(FString::Printf(TEXT("Bake writes %s"), *MeshPath), Mesh))
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
	const FString MeshPath = ExpectedActorMeshPath(Fixture, *Actor);

	// With -unattended the engine's save prompt cancels.
	const FCambrianaBakeResult Result = Cambriana::Editor::BakeLiveActor(*Actor);

	TestTrue(TEXT("The bake reports the grammar not saved"), Result.Status == ECambrianaBakeStatus::GrammarNotSaved);
	TestNull(TEXT("No static mesh is created"), FindObject<UStaticMesh>(nullptr, *MeshPath));
	return true;
}

#endif
