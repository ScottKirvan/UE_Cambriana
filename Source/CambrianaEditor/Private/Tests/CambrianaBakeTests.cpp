#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshResources.h"
#include "ToolMenus.h"
#include "UObject/ObjectRedirector.h"
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

	// SDD-rigging.md: sk.ls has 80 bones; Bake skeleton: one synthetic Root bone parents them.
	constexpr int32 SkSkeletonBones = 81;

	const TCHAR* const ErrorGrammarText = TEXT("20\n25\n50\n");

	int32 RenderedTriangles(UStaticMesh* Mesh)
	{
		const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
		return RenderData && RenderData->LODResources.Num() > 0 ? RenderData->LODResources[0].GetNumTriangles() : 0;
	}

	int32 RenderedTriangles(USkeletalMesh* Mesh)
	{
		const FSkeletalMeshRenderData* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
		return RenderData && RenderData->LODRenderData.Num() > 0 ? int32(RenderData->LODRenderData[0].GetTotalFaces()) : 0;
	}

	Cambriana::Editor::FAskToOverwrite Answer(EAppReturnType::Type Reply, int32& Asked)
	{
		return [Reply, &Asked](const FText&) { ++Asked; return Reply; };
	}

	/**
	 * Moves the mesh to MovedPackage and leaves a redirector to it at its old name, as the Content Browser's move does when the mesh
	 * has referencers it does not fix up (AssetRenameManager.cpp, which calls ObjectTools::RenameSingleObject to leave a redirector).
	 */
	UObjectRedirector* MoveLeavingRedirector(UObject& Mesh, const FString& MovedPackage)
	{
		const FString OldPath = Mesh.GetPathName();
		FPackageGroupName Moved;
		Moved.PackageName = MovedPackage;
		Moved.ObjectName = FPackageName::GetShortName(MovedPackage);
		TSet<UPackage*> RefusedToLoad;
		FText Error;
		if (!ObjectTools::RenameSingleObject(&Mesh, Moved, RefusedToLoad, Error, nullptr, true))
		{
			return nullptr;
		}
		return FindObject<UObjectRedirector>(nullptr, *OldPath);
	}

	/** Removes the moved package, in memory and on disk. */
	void RemoveMovedPackage(const FString& MovedPackage)
	{
		if (UPackage* Package = FindPackage(nullptr, *MovedPackage))
		{
			UnloadPackage(Package);
		}
		IFileManager::Get().Delete(*PackageFilename(*MovedPackage));
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename(*MovedPackage)), false, true);
	}

	/**
	 * Bakes the fixture's static mesh, moves it to MovedPackage leaving a redirector at its name, and optionally saves and unloads
	 * both, then changes the grammar and bakes the static mesh again; the redirector and the moved mesh must be left as they were.
	 */
	bool BakeOverRedirector(FAutomationTestBase& Test, FBakeFixture& Fixture, const FString& MovedPackage, bool bSavedAndUnloaded)
	{
		if (!Test.TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
		{
			return false;
		}
		UObjectRedirector* Redirector = MoveLeavingRedirector(*Fixture.FindMesh(), MovedPackage);
		if (!Test.TestTrue(TEXT("Moving the mesh leaves a redirector at its name"), Redirector && Redirector->DestinationObject))
		{
			return false;
		}
		const FString MovedObjectPath = Redirector->DestinationObject->GetPathName();
		if (bSavedAndUnloaded)
		{
			UPackage* RedirectorPackage = Redirector->GetPackage();
			UPackage* Moved = Redirector->DestinationObject->GetPackage();
			if (!Test.TestTrue(TEXT("The redirector and the moved mesh save"), UEditorLoadingAndSavingUtils::SavePackages({ RedirectorPackage, Moved }, false))
				|| !Test.TestTrue(TEXT("Both unload"), UnloadPackage(RedirectorPackage) && UnloadPackage(Moved)))
			{
				return false;
			}
		}
		Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });

		int32 Asked = 0;
		const FCambrianaBakeResult Result = Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked));

		Test.TestTrue(TEXT("The bake reports the name taken"), Result.Status == ECambrianaBakeStatus::NameTaken);
		Test.TestEqual(TEXT("Overwriting is not asked about"), Asked, 0);
		Test.TestNull(TEXT("No static mesh is baked"), Result.StaticMesh);
		const UObject* AtName = LoadObject<UObject>(nullptr, *Fixture.MeshObjectPath(), nullptr, LOAD_NoRedirects | LOAD_NoWarn | LOAD_Quiet);
		Test.TestTrue(TEXT("The redirector is still at the static mesh's name"), AtName && AtName->IsA<UObjectRedirector>());
		UStaticMesh* MovedMesh = LoadObject<UStaticMesh>(nullptr, *MovedObjectPath);
		if (Test.TestNotNull(TEXT("The moved mesh is there"), MovedMesh))
		{
			Test.TestEqual(TEXT("The moved mesh keeps its triangles at 8 sides"), RenderedTriangles(MovedMesh), SkTriangles);
			Test.TestFalse(TEXT("The moved mesh has no unsaved changes"), bSavedAndUnloaded && MovedMesh->GetPackage()->IsDirty());
		}
		return true;
	}
}

// ATS-41: Bake to Static Mesh on a grammar asset creates a static mesh and no skeletal mesh, skeleton or physics asset.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeStaticMeshTest,
	"Cambriana.Editor.Bake.BakeToStaticMeshWritesOnlyTheStaticMeshBesideTheGrammarWithItsTrianglesAndOneSlotPerMaterialId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeStaticMeshTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeSk"));
	const FCambrianaBakeResult Result = Fixture.BakeStatic();

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	UStaticMesh* Mesh = Fixture.FindMesh();
	if (!TestNotNull(TEXT("A static mesh SM_BakeSk is in the folder BakeSk beside the grammar"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("The result is that static mesh"), Result.StaticMesh == Mesh);
	TestTrue(TEXT("The static mesh is saved to disk"), Fixture.MeshFileExists());
	TestFalse(TEXT("The static mesh's package has no unsaved changes"), Mesh->GetPackage()->IsDirty());
	TestFalse(TEXT("The grammar, never saved, bakes without being saved"), IFileManager::Get().FileExists(*PackageFilename(*Fixture.GrammarPackage)));
	TestEqual(TEXT("Triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestEqual(TEXT("One material slot per material ID"), Mesh->GetStaticMaterials().Num(), SkMaterialIds);
	TestTrue(TEXT("The result has no skeletal mesh, skeleton or physics asset"), !Result.SkeletalMesh && !Result.Skeleton && !Result.PhysicsAsset);
	TestTrue(TEXT("No skeletal mesh, skeleton or physics asset is created"), !Fixture.FindSkeletalMesh() && !Fixture.FindSkeleton() && !Fixture.FindPhysicsAsset());
	TestFalse(TEXT("None is saved"), Fixture.AnySkeletalFileExists() || Fixture.PhysicsAssetFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeMenuEntriesTest,
	"Cambriana.Editor.Bake.GrammarRightClickMenuHasBakeToStaticMeshThenBakeToSkeletalMeshAndNoBake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeMenuEntriesTest::RunTest(const FString& Parameters)
{
	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UCambrianaGrammar::StaticClass());
	FToolMenuSection* Section = Menu ? Menu->FindSection(Cambriana::Editor::BakeMenuSection) : nullptr;
	if (!TestNotNull(TEXT("The grammar asset's right-click menu has the asset actions section"), Section))
	{
		return false;
	}
	const int32 StaticIndex = Section->Blocks.IndexOfByPredicate([](const FToolMenuEntry& Block) { return Block.Name == Cambriana::Editor::BakeToStaticMeshMenuEntry; });
	const int32 SkeletalIndex = Section->Blocks.IndexOfByPredicate([](const FToolMenuEntry& Block) { return Block.Name == Cambriana::Editor::BakeToSkeletalMeshMenuEntry; });
	TestTrue(TEXT("Both entries are in that section"), StaticIndex != INDEX_NONE && SkeletalIndex != INDEX_NONE);
	TestTrue(TEXT("Bake to Static Mesh comes first, directly before Bake to Skeletal Mesh"), StaticIndex != INDEX_NONE && SkeletalIndex == StaticIndex + 1);
	const FToolMenuEntry* StaticEntry = Section->FindEntry(Cambriana::Editor::BakeToStaticMeshMenuEntry);
	const FToolMenuEntry* SkeletalEntry = Section->FindEntry(Cambriana::Editor::BakeToSkeletalMeshMenuEntry);
	if (StaticEntry && SkeletalEntry)
	{
		TestEqual(TEXT("The first is labelled Bake to Static Mesh"), StaticEntry->Label.Get().ToString(), FString(TEXT("Bake to Static Mesh")));
		TestEqual(TEXT("The second is labelled Bake to Skeletal Mesh"), SkeletalEntry->Label.Get().ToString(), FString(TEXT("Bake to Skeletal Mesh")));
	}
	TestNull(TEXT("The single Bake entry is gone"), Section->FindEntry(TEXT("CambrianaGrammar_Bake")));
	return true;
}

namespace
{
	/** Runs the menu entry named EntryName on the fixture's grammar. */
	bool RunMenuEntry(const FBakeFixture& Fixture, FName EntryName)
	{
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UCambrianaGrammar::StaticClass());
		FToolMenuSection* Section = Menu ? Menu->FindSection(Cambriana::Editor::BakeMenuSection) : nullptr;
		FToolMenuEntry* Entry = Section ? Section->FindEntry(EntryName) : nullptr;
		if (!Entry)
		{
			return false;
		}
		UContentBrowserAssetContextMenuContext* Context = NewObject<UContentBrowserAssetContextMenuContext>();
		Context->SelectedAssets.Add(FAssetData(Fixture.Grammar));
		bool bExecuted = false;
		RunWithoutSavePrompt([&] { bExecuted = Entry->TryExecuteToolUIAction(FToolMenuContext(Context)); });
		return bExecuted;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeMenuStaticTest,
	"Cambriana.Editor.Bake.BakeToStaticMeshFromTheMenuWritesOnlyTheStaticMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeMenuStaticTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeStaticFromMenu"));
	TestTrue(TEXT("Bake to Static Mesh has an action that runs"), RunMenuEntry(Fixture, Cambriana::Editor::BakeToStaticMeshMenuEntry));

	TestEqual(TEXT("It writes SM_BakeStaticFromMenu with its triangles"), RenderedTriangles(Fixture.FindMesh()), SkTriangles);
	TestTrue(TEXT("The static mesh is saved to disk"), Fixture.MeshFileExists());
	TestTrue(TEXT("It writes no skeletal mesh, skeleton or physics asset"), !Fixture.FindSkeletalMesh() && !Fixture.FindSkeleton() && !Fixture.FindPhysicsAsset());
	TestFalse(TEXT("None is saved"), Fixture.AnySkeletalFileExists() || Fixture.PhysicsAssetFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeMenuSkeletalTest,
	"Cambriana.Editor.Bake.BakeToSkeletalMeshFromTheMenuWritesOnlyTheSkeletalOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeMenuSkeletalTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeSkeletalFromMenu"));
	TestTrue(TEXT("Bake to Skeletal Mesh has an action that runs"), RunMenuEntry(Fixture, Cambriana::Editor::BakeToSkeletalMeshMenuEntry));

	TestEqual(TEXT("It writes SK_BakeSkeletalFromMenu with its triangles"), RenderedTriangles(Fixture.FindSkeletalMesh()), SkTriangles);
	TestTrue(TEXT("The skeletal mesh and skeleton are saved to disk"), Fixture.SkeletalFilesExist());
	TestTrue(TEXT("It writes the physics asset, saved to disk"), Fixture.FindPhysicsAsset() && Fixture.PhysicsAssetFileExists());
	TestNull(TEXT("It writes no static mesh"), Fixture.FindMesh());
	TestFalse(TEXT("No static mesh is saved"), Fixture.MeshFileExists());
	return true;
}

// ATS-40: Bake to Skeletal Mesh on sk.ls creates a skeletal mesh of 80 bones plus Root, 4 material slots, a skeleton and a physics asset, and no static mesh.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeSkeletalTest,
	"Cambriana.Editor.Bake.BakeToSkeletalMeshWritesTheSkeletonSkinnedMeshAndPhysicsAssetBesideTheGrammarWithItsBonesAndSlotsAndNoStaticMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeSkeletalTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeSkeletal"));
	const FCambrianaBakeResult Result = Fixture.BakeSkeletal();

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	USkeleton* Skeleton = Fixture.FindSkeleton();
	if (!TestNotNull(TEXT("A skeletal mesh SK_BakeSkeletal is in the folder BakeSkeletal beside the grammar"), Mesh)
		|| !TestNotNull(TEXT("A skeleton SKEL_BakeSkeletal is in the same folder"), Skeleton))
	{
		return false;
	}
	TestTrue(TEXT("The result is that skeletal mesh and skeleton"), Result.SkeletalMesh == Mesh && Result.Skeleton == Skeleton);
	TestTrue(TEXT("The skeletal mesh uses the skeleton"), Mesh->GetSkeleton() == Skeleton);
	TestTrue(TEXT("Both are saved to disk"), Fixture.SkeletalFilesExist());
	TestFalse(TEXT("Neither package has unsaved changes"), Mesh->GetPackage()->IsDirty() || Skeleton->GetPackage()->IsDirty());
	TestEqual(TEXT("The skeletal mesh has sk.ls's 80 bones plus Root"), Mesh->GetRefSkeleton().GetNum(), SkSkeletonBones);
	TestEqual(TEXT("So has the skeleton"), Skeleton->GetReferenceSkeleton().GetNum(), SkSkeletonBones);
	TestEqual(TEXT("Its first bone is Root"), Mesh->GetRefSkeleton().GetBoneName(0), FName(TEXT("Root")));
	TestEqual(TEXT("One material slot per material ID"), Mesh->GetMaterials().Num(), SkMaterialIds);
	TestEqual(TEXT("Triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestTrue(TEXT("The result is the physics asset, saved to disk"), Result.PhysicsAsset && Result.PhysicsAsset == Fixture.FindPhysicsAsset() && Fixture.PhysicsAssetFileExists());
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("No static mesh is saved"), Fixture.MeshFileExists());
	TestNull(TEXT("The result has no static mesh"), Result.StaticMesh);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeErrorsTest,
	"Cambriana.Editor.Bake.GrammarWithErrorsWritesNothingWithEitherChoiceAndLeavesTheBakedMeshesUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeErrorsTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("Grammar error at line"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
	AddExpectedMessagePlain(TEXT("the grammar has errors; nothing is baked"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
	FBakeFixture Fixture(TEXT("BakeErrors"));
	if (!TestTrue(TEXT("The first static mesh bake succeeds"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The first skeletal mesh bake succeeds"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	USkeletalMesh* SkeletalMesh = Fixture.FindSkeletalMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.GrammarText = ErrorGrammarText; });

	int32 Asked = 0;
	const FCambrianaBakeResult StaticResult = Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked));
	const FCambrianaBakeResult SkeletalResult = Fixture.BakeSkeletal(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("Bake to Static Mesh reports nothing to build"), StaticResult.Status == ECambrianaBakeStatus::NothingToBuild);
	TestTrue(TEXT("Bake to Skeletal Mesh reports nothing to build"), SkeletalResult.Status == ECambrianaBakeStatus::NothingToBuild);
	TestEqual(TEXT("Overwriting is not asked about"), Asked, 0);
	TestTrue(TEXT("The baked mesh is still there"), Fixture.FindMesh() == Mesh);
	TestEqual(TEXT("The baked mesh keeps its triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestTrue(TEXT("The baked skeletal mesh is still there"), SkeletalMesh && Fixture.FindSkeletalMesh() == SkeletalMesh);
	TestEqual(TEXT("The baked skeletal mesh keeps its triangles"), RenderedTriangles(SkeletalMesh), SkTriangles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeEmptyTextTest,
	"Cambriana.Editor.Bake.EmptyGrammarTextWritesNothingWithEitherChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeEmptyTextTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("the grammar builds no triangles; nothing is baked"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
	FBakeFixture Fixture(TEXT("BakeEmpty"));
	Fixture.Grammar->GrammarText.Reset();

	TestTrue(TEXT("Bake to Static Mesh reports nothing to build"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::NothingToBuild);
	TestTrue(TEXT("Bake to Skeletal Mesh reports nothing to build"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::NothingToBuild);
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("No static mesh is saved"), Fixture.MeshFileExists());
	TestNull(TEXT("No skeletal mesh is created"), Fixture.FindSkeletalMesh());
	TestFalse(TEXT("No skeletal output is saved"), Fixture.AnySkeletalFileExists());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeOverwriteTest,
	"Cambriana.Editor.Bake.BakeToStaticMeshRebuildsOnlyTheStaticMeshInPlaceWhenOverwriteIsAcceptedAndAsksOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeOverwriteTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeOverwrite"));
	int32 Asked = 0;
	if (!TestTrue(TEXT("The first skeletal bake succeeds"), Fixture.BakeSkeletal(Answer(EAppReturnType::Yes, Asked)).Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	USkeletalMesh* SkeletalMesh = Fixture.FindSkeletalMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });

	const FCambrianaBakeResult First = Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked));
	TestTrue(TEXT("The first static bake succeeds"), First.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("Existing skeletal outputs do not make a first static bake ask"), Asked, 0);
	UStaticMesh* Mesh = Fixture.FindMesh();
	if (!TestNotNull(TEXT("The static mesh is written"), Mesh))
	{
		return false;
	}
	TestEqual(TEXT("The static mesh has the saved grammar's new triangles"), RenderedTriangles(Mesh), SkTrianglesAtSixSides);
	TestEqual(TEXT("The skeletal mesh was not touched"), RenderedTriangles(SkeletalMesh), SkTriangles);

	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 8; });
	const FCambrianaBakeResult Result = Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("Overwriting is asked once"), Asked, 1);
	TestTrue(TEXT("The same static mesh object is rebuilt, so references to it hold"), Fixture.FindMesh() == Mesh && Result.StaticMesh == Mesh);
	TestEqual(TEXT("It has the saved grammar's new triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestFalse(TEXT("It is saved"), Mesh->GetPackage()->IsDirty());
	TestTrue(TEXT("The skeletal mesh is still the same object, not rebuilt"), SkeletalMesh && Fixture.FindSkeletalMesh() == SkeletalMesh && !Result.SkeletalMesh);
	TestEqual(TEXT("The skeletal mesh keeps its triangles"), RenderedTriangles(SkeletalMesh), SkTriangles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeSkeletalOverwriteTest,
	"Cambriana.Editor.Bake.BakeToSkeletalMeshRebuildsOnlyTheSkeletalOutputsInPlaceWhenOverwriteIsAcceptedAndAsksOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeSkeletalOverwriteTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeSkeletalOverwrite"));
	int32 Asked = 0;
	if (!TestTrue(TEXT("The first static bake succeeds"), Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked)).Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });

	TestTrue(TEXT("The first skeletal bake succeeds"), Fixture.BakeSkeletal(Answer(EAppReturnType::Yes, Asked)).Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("An existing static mesh does not make a first skeletal bake ask"), Asked, 0);
	USkeletalMesh* SkeletalMesh = Fixture.FindSkeletalMesh();
	USkeleton* Skeleton = Fixture.FindSkeleton();
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	TestEqual(TEXT("The static mesh was not touched"), RenderedTriangles(Mesh), SkTriangles);

	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 8; });
	const FCambrianaBakeResult Result = Fixture.BakeSkeletal(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("Overwriting is asked once for all the skeletal outputs"), Asked, 1);
	TestTrue(TEXT("The same skeletal mesh object is rebuilt"), SkeletalMesh && Fixture.FindSkeletalMesh() == SkeletalMesh && Result.SkeletalMesh == SkeletalMesh);
	TestTrue(TEXT("The same skeleton object is kept"), Skeleton && Fixture.FindSkeleton() == Skeleton && Result.Skeleton == Skeleton);
	TestTrue(TEXT("The same physics asset object is rebuilt"), PhysicsAsset && Fixture.FindPhysicsAsset() == PhysicsAsset && Result.PhysicsAsset == PhysicsAsset);
	TestTrue(TEXT("The skeletal mesh still uses the skeleton"), SkeletalMesh && SkeletalMesh->GetSkeleton() == Skeleton);
	TestEqual(TEXT("The skeletal mesh has the saved grammar's new triangles"), RenderedTriangles(SkeletalMesh), SkTriangles);
	TestFalse(TEXT("The skeletal outputs are saved"), !SkeletalMesh || !Skeleton || SkeletalMesh->GetPackage()->IsDirty() || Skeleton->GetPackage()->IsDirty());
	TestTrue(TEXT("The static mesh is still the same object, not rebuilt"), Fixture.FindMesh() == Mesh && !Result.StaticMesh);
	TestEqual(TEXT("The static mesh keeps its triangles"), RenderedTriangles(Mesh), SkTriangles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeOverwriteDeclinedTest,
	"Cambriana.Editor.Bake.ExistingOutputsOfTheChosenBakeAreUnchangedWhenOverwriteIsDeclined",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeOverwriteDeclinedTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeDeclined"));
	if (!TestTrue(TEXT("The first static bake succeeds"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The first skeletal bake succeeds"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	USkeletalMesh* SkeletalMesh = Fixture.FindSkeletalMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });

	int32 Asked = 0;
	const FCambrianaBakeResult StaticResult = Fixture.BakeStatic(Answer(EAppReturnType::No, Asked));
	TestTrue(TEXT("Bake to Static Mesh reports the overwrite declined"), StaticResult.Status == ECambrianaBakeStatus::OverwriteDeclined);
	TestEqual(TEXT("Overwriting is asked once"), Asked, 1);
	TestTrue(TEXT("The static mesh is still there"), Fixture.FindMesh() == Mesh);
	TestEqual(TEXT("It keeps its triangles"), RenderedTriangles(Mesh), SkTriangles);
	TestFalse(TEXT("It has no unsaved changes"), Mesh->GetPackage()->IsDirty());

	const FCambrianaBakeResult SkeletalResult = Fixture.BakeSkeletal(Answer(EAppReturnType::No, Asked));
	TestTrue(TEXT("Bake to Skeletal Mesh reports the overwrite declined"), SkeletalResult.Status == ECambrianaBakeStatus::OverwriteDeclined);
	TestEqual(TEXT("Overwriting is asked once more"), Asked, 2);
	TestTrue(TEXT("The skeletal mesh is still there"), SkeletalMesh && Fixture.FindSkeletalMesh() == SkeletalMesh);
	TestEqual(TEXT("The skeletal mesh keeps its triangles"), RenderedTriangles(SkeletalMesh), SkTriangles);
	TestFalse(TEXT("The skeletal mesh has no unsaved changes"), !SkeletalMesh || SkeletalMesh->GetPackage()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeNameTakenTest,
	"Cambriana.Editor.Bake.BakeToStaticMeshLeavesAnAssetOfAnotherClassAtTheStaticMeshNameAndBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeNameTakenTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is not a static mesh; nothing is baked"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeNameTaken"));
	UPackage* Package = CreatePackage(*Fixture.MeshPackage);
	UCambrianaGrammar* Occupant = NewObject<UCambrianaGrammar>(Package, *(TEXT("SM_") + Fixture.Name), RF_Public | RF_Standalone);

	int32 Asked = 0;
	const FCambrianaBakeResult Result = Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports the name taken"), Result.Status == ECambrianaBakeStatus::NameTaken);
	TestEqual(TEXT("Overwriting is not asked about"), Asked, 0);
	TestTrue(TEXT("The other asset is untouched"), FindObject<UObject>(nullptr, *Fixture.MeshObjectPath()) == Occupant && IsValid(Occupant));
	TestNull(TEXT("No static mesh is created"), Fixture.FindMesh());
	TestFalse(TEXT("Nothing is saved at the name"), Fixture.MeshFileExists());
	TestTrue(TEXT("Bake to Skeletal Mesh is not blocked by it"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeSkeletalNameTakenTest,
	"Cambriana.Editor.Bake.BakeToSkeletalMeshLeavesAnAssetOfAnotherClassAtTheSkeletalMeshNameAndBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeSkeletalNameTakenTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is not a skeletal mesh; nothing is baked"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeSkeletalNameTaken"));
	UPackage* Package = CreatePackage(*Fixture.SkeletalMeshPackage);
	UCambrianaGrammar* Occupant = NewObject<UCambrianaGrammar>(Package, *(TEXT("SK_") + Fixture.Name), RF_Public | RF_Standalone);

	int32 Asked = 0;
	const FCambrianaBakeResult Result = Fixture.BakeSkeletal(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports the name taken"), Result.Status == ECambrianaBakeStatus::NameTaken);
	TestEqual(TEXT("Overwriting is not asked about"), Asked, 0);
	TestTrue(TEXT("The other asset is untouched"),
		FindObject<UObject>(nullptr, *(Fixture.SkeletalMeshPackage + TEXT(".SK_") + Fixture.Name)) == Occupant && IsValid(Occupant));
	TestNull(TEXT("No skeleton is created"), Fixture.FindSkeleton());
	TestNull(TEXT("No physics asset is created"), Fixture.FindPhysicsAsset());
	TestFalse(TEXT("Nothing is saved"), Fixture.MeshFileExists() || Fixture.AnySkeletalFileExists() || Fixture.PhysicsAssetFileExists());
	return true;
}

// SDD-unreal.md: a redirector at an output's name is an asset of another class there, as the engine's import and asset creation
// treat it without following it (AssetTools.cpp, ImportAssetsInternal with AssetTools.FollowRedirectorsWhenImporting off, and
// CanCreateAsset).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeRedirectorTest,
	"Cambriana.Editor.Bake.BakeToStaticMeshLeavesARedirectorAtTheStaticMeshNameAndTheMovedMeshAloneAndBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeRedirectorTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is a redirector to"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeRedirector"));
	const FString MovedPackage = FString(BakeTestRoot) / TEXT("BakeRedirectorMoved") / TEXT("SM_Moved");
	ON_SCOPE_EXIT { RemoveMovedPackage(MovedPackage); };
	return BakeOverRedirector(*this, Fixture, MovedPackage, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeSavedRedirectorTest,
	"Cambriana.Editor.Bake.BakeToStaticMeshLeavesASavedUnloadedRedirectorAtTheStaticMeshNameAndTheMovedMeshAloneAndBakesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeSavedRedirectorTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is a redirector to"), ELogVerbosity::Error);
	FBakeFixture Fixture(TEXT("BakeSavedRedirector"));
	const FString MovedPackage = FString(BakeTestRoot) / TEXT("BakeSavedRedirectorMoved") / TEXT("SM_Moved");
	ON_SCOPE_EXIT { RemoveMovedPackage(MovedPackage); };
	return BakeOverRedirector(*this, Fixture, MovedPackage, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeUnsavedGrammarTest,
	"Cambriana.Editor.Bake.BakeBakesTheGrammarAssetsUnsavedChangesInMemoryWithoutSavingThem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeUnsavedGrammarTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("BakeUnsaved"));
	UPackage* GrammarPackage = Fixture.Grammar->GetPackage();
	if (!TestTrue(TEXT("The grammar saves"), UEditorLoadingAndSavingUtils::SavePackages({ GrammarPackage }, false)))
	{
		return false;
	}
	Fixture.Edit([](UCambrianaGrammar& Grammar) { Grammar.Sides = 6; });
	const FDateTime SavedAt = IFileManager::Get().GetTimeStamp(*PackageFilename(*Fixture.GrammarPackage));

	int32 Asked = 0;
	const FCambrianaBakeResult Result = Fixture.BakeStatic(Answer(EAppReturnType::Yes, Asked));

	TestTrue(TEXT("The bake reports success"), Result.Status == ECambrianaBakeStatus::Baked);
	TestEqual(TEXT("The static mesh has the unsaved 6 sides"), RenderedTriangles(Fixture.FindMesh()), SkTrianglesAtSixSides);
	TestTrue(TEXT("The grammar still has its unsaved changes"), GrammarPackage->IsDirty());
	TestTrue(TEXT("The grammar's package file is not written"), IFileManager::Get().GetTimeStamp(*PackageFilename(*Fixture.GrammarPackage)) == SavedAt);
	TestEqual(TEXT("Nothing asks to overwrite"), Asked, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeSettingSaveTest,
	"Cambriana.Editor.Save.GrammarBakeSettingsSurviveSavingAndReloading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeSettingSaveTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Physics stiffness is 25000 by default (SDD-rigging.md, Physics)"), GetDefault<UCambrianaGrammar>()->Bake.PhysicsStiffness, 25000.0f);

	FBakeFixture Fixture(TEXT("BakeSettingSave"));
	Fixture.Grammar->Bake.PhysicsStiffness = 1234.5f;
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
		TestEqual(TEXT("Physics stiffness is still 1234.5"), Reloaded->Bake.PhysicsStiffness, 1234.5f);
	}
	return true;
}

#endif
