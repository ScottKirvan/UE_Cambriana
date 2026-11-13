#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "StaticMeshResources.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// SDD-surfacing.md, Tubes: sk.ls at 8 sides has 1,280 triangles. Each of its 80 segments (four branches of 20, one per
	// generation) is a tube of 2 triangles per side without caps, so at 6 sides it has 80 * 2 * 6 = 960.
	constexpr int32 SkTriangles = 1280;
	constexpr int32 SkTrianglesAtSixSides = 960;

	// sk.ls draws its four branches after `c` once, twice, three and four times from colour 0, and draws nothing outside them
	// (FMT-legacy-ls: `c` adds 1 to the colour, `[` pushes it), so it has four material IDs, 1 to 4.
	constexpr int32 SkMaterialIds = 4;

	const TCHAR* const ErrorGrammarText = TEXT("20\n25\n50\n");

	int32 RenderedTriangles(UStaticMesh* Mesh)
	{
		const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
		return RenderData && RenderData->LODResources.Num() > 0 ? RenderData->LODResources[0].GetNumTriangles() : 0;
	}

	Cambriana::Editor::FAskToOverwrite Answer(EAppReturnType::Type Reply, int32& Asked)
	{
		return [Reply, &Asked](const FText&) { ++Asked; return Reply; };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeStaticMeshTest,
	"Cambriana.Editor.Bake.StaticMeshOutputWritesTheStaticMeshBesideTheGrammarWithItsTrianglesAndOneSlotPerMaterialId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeStaticMeshTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeSk"));
	const FCambrianaBakeResult Result = Fixture.Bake();

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	UStaticMesh* Mesh = Fixture.FindMesh();
	if (!TestNotNull(TEXT("A static mesh SM_BakeSk is in the folder BakeSk beside the grammar"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The result is that static mesh"), Result.StaticMesh == Mesh);
	TestTrue(TEXT("The static mesh is saved to disk"), Fixture.MeshFileExists());
	TestFalse(TEXT("The static mesh's package has no unsaved changes"), Mesh->GetPackage()->IsDirty());
	TestTrue(TEXT("The grammar, never saved, is saved before it bakes"),
		IFileManager::Get().FileExists(*PackageFilename(*Fixture.GrammarPackage)) && !Fixture.Grammar->GetPackage()->IsDirty());
	TestEqual(TEXT("Triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestEqual(TEXT("One material slot per material ID"), Mesh->GetStaticMaterials().Num(), SkMaterialIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeMenuTest,
	"Cambriana.Editor.Bake.GrammarRightClickMenuBakeWritesTheStaticMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeMenuTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeFromMenu"));

	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UCambrianaGrammar::StaticClass());
	FToolMenuSection* Section = Menu ? Menu->FindSection(Cambriana::Editor::BakeMenuSection) : nullptr;
	FToolMenuEntry* Entry = Section ? Section->FindEntry(Cambriana::Editor::BakeMenuEntry) : nullptr;
	if (!TestNotNull(TEXT("The grammar asset's right-click menu has Bake"), Entry))
	{
		return false;
	}
	TestEqual(TEXT("The entry is labelled Bake"), Entry->Label.Get().ToString(), FString(TEXT("Bake")));

	UContentBrowserAssetContextMenuContext* Context = NewObject<UContentBrowserAssetContextMenuContext>();
	Context->SelectedAssets.Add(FAssetData(Fixture.Grammar));
	bool bExecuted = false;
	RunWithoutSavePrompt([&] { bExecuted = Entry->TryExecuteToolUIAction(FToolMenuContext(Context)); });
	TestTrue(TEXT("Bake has an action that runs"), bExecuted);

	UStaticMesh* Mesh = Fixture.FindMesh();
	TestNotNull(TEXT("Bake from the menu writes SM_BakeFromMenu beside the grammar"), Mesh);
	TestEqual(TEXT("Triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestTrue(TEXT("The static mesh is saved to disk"), Fixture.MeshFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeStaticMeshOffTest,
	"Cambriana.Editor.Bake.StaticMeshOutputOffWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeStaticMeshOffTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("no bake output is selected"), ELogVerbosity::Warning);
	FBakeFixture Fixture(TEXT("BakeOff"));
	Fixture.Grammar->Bake.bStaticMesh = false;

	const FCambrianaBakeResult Result = Fixture.Bake();

	TestTrue(TEXT("The bake reports that no output is selected"), Result.Status == ECambrianaBakeStatus::NoOutputSelected);
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("No static mesh is saved"), Fixture.MeshFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeErrorsTest,
	"Cambriana.Editor.Bake.GrammarWithErrorsWritesNothingAndLeavesTheBakedMeshUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeErrorsTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("Grammar error at line"), ELogVerbosity::Error);
	AddExpectedMessagePlain(TEXT("the grammar has errors; nothing is baked"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeErrors"));
	if (!TestTrue(TEXT("The first bake succeeds"), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.GrammarText = ErrorGrammarText; });

	int32 Asked = 0;
	const FCambrianaBakeResult Result = Fixture.Bake(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports nothing to build"), Result.Status == ECambrianaBakeStatus::NothingToBuild);
	TestEqual(TEXT("Overwriting is not asked about"), Asked, 0);
	TestTrue(TEXT("The baked mesh is still there"), Fixture.FindMesh() == Mesh);
	TestEqual(TEXT("The baked mesh keeps its triangles"), RenderedTriangles(Mesh), SkTriangles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeEmptyTextTest,
	"Cambriana.Editor.Bake.EmptyGrammarTextWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeEmptyTextTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("the grammar builds no triangles; nothing is baked"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeEmpty"));
	Fixture.Grammar->GrammarText.Reset();

	const FCambrianaBakeResult Result = Fixture.Bake();

	TestTrue(TEXT("The bake reports nothing to build"), Result.Status == ECambrianaBakeStatus::NothingToBuild);
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("No static mesh is saved"), Fixture.MeshFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeOverwriteTest,
	"Cambriana.Editor.Bake.ExistingStaticMeshIsRebuiltInPlaceWhenOverwriteIsAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeOverwriteTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeOverwrite"));
	int32 Asked = 0;
	if (!TestTrue(TEXT("The first bake succeeds"), Fixture.Bake(Answer(EAppReturnType::Yes, Asked)).Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	TestEqual(TEXT("A first bake does not ask to overwrite"), Asked, 0);
	UStaticMesh* Mesh = Fixture.FindMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });

	const FCambrianaBakeResult Result = Fixture.Bake(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("Overwriting is asked once"), Asked, 1);
	TestTrue(TEXT("The same static mesh object is rebuilt, so references to it hold"), Fixture.FindMesh() == Mesh && Result.StaticMesh == Mesh);
	TestEqual(TEXT("It has the saved grammar's new triangles"), RenderedTriangles(Mesh), SkTrianglesAtSixSides);
	TestFalse(TEXT("It is saved"), Mesh->GetPackage()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeOverwriteDeclinedTest,
	"Cambriana.Editor.Bake.ExistingStaticMeshIsUnchangedWhenOverwriteIsDeclined",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeOverwriteDeclinedTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeDeclined"));
	if (!TestTrue(TEXT("The first bake succeeds"), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });

	int32 Asked = 0;
	const FCambrianaBakeResult Result = Fixture.Bake(Answer(EAppReturnType::No, Asked));

	TestTrue(TEXT("The bake reports the overwrite declined"), Result.Status == ECambrianaBakeStatus::OverwriteDeclined);
	TestEqual(TEXT("Overwriting is asked once"), Asked, 1);
	TestTrue(TEXT("The static mesh is still there"), Fixture.FindMesh() == Mesh);
	TestEqual(TEXT("It keeps its triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestFalse(TEXT("It has no unsaved changes"), Mesh->GetPackage()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeNameTakenTest,
	"Cambriana.Editor.Bake.AssetOfAnotherClassWithTheStaticMeshNameIsLeftAndNothingIsBaked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeNameTakenTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is not a static mesh; nothing is baked"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeNameTaken"));
	UPackage* Package = CreatePackage(*Fixture.MeshPackage);
	UCambrianaGrammar* Occupant = NewObject<UCambrianaGrammar>(Package, *(TEXT("SM_") + Fixture.Name), RF_Public | RF_Standalone);

	int32 Asked = 0;
	const FCambrianaBakeResult Result = Fixture.Bake(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports the name taken"), Result.Status == ECambrianaBakeStatus::NameTaken);
	TestEqual(TEXT("Overwriting is not asked about"), Asked, 0);
	TestTrue(TEXT("The other asset is untouched"), FindObject<UObject>(nullptr, *Fixture.MeshObjectPath()) == Occupant && IsValid(Occupant));
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("Nothing is saved at the name"), Fixture.MeshFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeUnsavedGrammarTest,
	"Cambriana.Editor.Bake.GrammarWhoseSaveIsCancelledIsNotBaked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeUnsavedGrammarTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("the grammar is not saved; nothing is baked"), ELogVerbosity::Warning);
	FBakeFixture Fixture(TEXT("BakeUnsaved"));

	// With -unattended the engine's save prompt cancels.
	const FCambrianaBakeResult Result = Cambriana::Editor::BakeGrammar(*Fixture.Grammar, nullptr);

	TestTrue(TEXT("The bake reports the grammar not saved"), Result.Status == ECambrianaBakeStatus::GrammarNotSaved);
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("No static mesh is saved"), Fixture.MeshFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeSettingSaveTest,
	"Cambriana.Editor.Save.GrammarBakeSettingsSurviveSavingAndReloading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeSettingSaveTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Static mesh output is off by default"), GetDefault<UCambrianaGrammar>()->Bake.bStaticMesh);

	FBakeFixture Fixture(TEXT("BakeSettingSave"));
	UPackage* Package = Fixture.Grammar->GetPackage();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	if (!TestTrue(TEXT("The package saves to disk"), UPackage::SavePackage(Package, Fixture.Grammar, *PackageFilename(*Fixture.GrammarPackage), SaveArgs)))
	{
		return false;
	}
	TestTrue(TEXT("The package unloads"), UnloadPackage(Package));

	UPackage* Loaded = LoadPackage(nullptr, *Fixture.GrammarPackage, LOAD_None);
	const UCambrianaGrammar* Reloaded = Loaded ? FindObject<UCambrianaGrammar>(Loaded, *Fixture.Name) : nullptr;
	if (TestNotNull(TEXT("The grammar asset loads again"), Reloaded))
	{
		TestTrue(TEXT("Static mesh output is still on"), Reloaded->Bake.bStaticMesh);
	}
	return true;
}

#endif
