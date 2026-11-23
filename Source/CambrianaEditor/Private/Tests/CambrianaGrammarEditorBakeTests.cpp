#include "CambrianaBake.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshResources.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// SDD-surfacing.md, Tubes: sk.ls at 8 sides has 1,280 triangles; at 6 sides 80 segments * 2 triangles * 6 sides = 960.
	constexpr int32 SkTriangles = 1280;
	constexpr int32 SkTrianglesAtSixSides = 960;
	constexpr int32 EditedSides = 6;

	// SDD-unreal.md: the choices are two entries of a Bake combo on the grammar editor's toolbar.
	const FName ToolbarMenuName(TEXT("AssetEditor.CambrianaGrammarEditor.ToolBar"));
	const FName BakeMenuName(TEXT("AssetEditor.CambrianaGrammarEditor.ToolBar.Bake"));
	const FName BakeComboEntry(TEXT("Bake"));
	const FName BakeToStaticMeshEntry(TEXT("BakeToStaticMesh"));
	const FName BakeToSkeletalMeshEntry(TEXT("BakeToSkeletalMesh"));

	int32 RenderedTriangles(UStaticMesh* Mesh)
	{
		const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
		return RenderData && RenderData->LODResources.Num() > 0 ? RenderData->LODResources[0].GetNumTriangles() : 0;
	}

	FToolMenuEntry* FindEntry(FName MenuName, FName EntryName)
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
		for (FToolMenuSection& Section : Menu->Sections)
		{
			if (FToolMenuEntry* Entry = Section.FindEntry(EntryName))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	/** Runs the Bake menu entry on the grammar editor, as its toolbar does, with the engine's save prompts saving without asking. */
	bool RunBakeEntry(FCambrianaGrammarEditorToolkit& Toolkit, FName EntryName)
	{
		FToolMenuEntry* Entry = FindEntry(BakeMenuName, EntryName);
		if (!Entry)
		{
			return false;
		}
		UAssetEditorToolkitMenuContext* Context = NewObject<UAssetEditorToolkitMenuContext>();
		Context->Toolkit = StaticCastSharedRef<FAssetEditorToolkit>(Toolkit.AsShared());
		bool bExecuted = false;
		RunWithoutSavePrompt([&] { bExecuted = Entry->TryExecuteToolUIAction(FToolMenuContext(Context)); });
		return bExecuted;
	}

	struct FEditedGrammar
	{
		explicit FEditedGrammar(const TCHAR* Name) : Fixture(Name) {}
		~FEditedGrammar() { CloseGrammarEditor(Fixture.Grammar); }

		/** Opens the editor on the grammar and edits its sides in the working copy. */
		FCambrianaGrammarEditorToolkit* OpenAndEdit()
		{
			Toolkit = OpenGrammarEditor(Fixture.Grammar);
			if (Toolkit)
			{
				EditGrammarSides(Toolkit->GetWorkingCopy(), EditedSides);
			}
			return Toolkit;
		}

		FBakeFixture Fixture;
		FCambrianaGrammarEditorToolkit* Toolkit = nullptr;
	};
}

// ATS-48: both Bake choices are offered in the grammar editor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeChoicesTest,
	"Cambriana.Editor.GrammarEditor.Bake.TheToolbarOffersBakeToStaticMeshAndBakeToSkeletalMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeChoicesTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("The toolbar has a Bake combo"), FindEntry(ToolbarMenuName, BakeComboEntry));
	const FToolMenuEntry* Static = FindEntry(BakeMenuName, BakeToStaticMeshEntry);
	const FToolMenuEntry* Skeletal = FindEntry(BakeMenuName, BakeToSkeletalMeshEntry);
	if (!TestNotNull(TEXT("The Bake menu has Bake to Static Mesh"), Static) || !TestNotNull(TEXT("The Bake menu has Bake to Skeletal Mesh"), Skeletal))
	{
		return false;
	}
	TestEqual(TEXT("First label"), Static->Label.Get().ToString(), FString(TEXT("Bake to Static Mesh")));
	TestEqual(TEXT("Second label"), Skeletal->Label.Get().ToString(), FString(TEXT("Bake to Skeletal Mesh")));
	return true;
}

namespace
{
	// SDD-surfacing.md, Tubes: TypedGrammarText grows A -> F A twice, a chain of 2 segments, each 2 triangles per side, at 8 sides.
	constexpr int32 TypedTriangles = 2 * 2 * 8;

	/** Counts the unsaved edits prompt's questions, answering No. */
	void CountQuestions(FCambrianaGrammarEditorToolkit& Toolkit, int32& Asked)
	{
		Toolkit.SetUnsavedEditsPrompt([&Asked](EAppMsgType::Type, const FText&) { ++Asked; return EAppReturnType::No; });
	}

	/** ATS-48: the asset and the editor's unsaved edits are as they were before the bake. */
	void TestAssetAndEditsUnchanged(FAutomationTestBase& Test, const FEditedGrammar& Edited, const FString& TextBefore, int32 SidesBefore)
	{
		Test.TestEqual(TEXT("The asset keeps its sides"), Edited.Fixture.Grammar->Sides, SidesBefore);
		Test.TestEqual(TEXT("The asset keeps its text"), Edited.Fixture.Grammar->GrammarText, TextBefore);
		Test.TestFalse(TEXT("The asset is not saved"), IFileManager::Get().FileExists(*PackageFilename(*Edited.Fixture.GrammarPackage)));
		Test.TestTrue(TEXT("The editor still has its unsaved edits"), Edited.Toolkit->HasUnsavedEdits());
	}
}

// ATS-48: with unsaved edits, nothing asks to save.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeAsksNothingTest,
	"Cambriana.Editor.GrammarEditor.Bake.WithUnsavedEditsNeitherChoiceAsksAnything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeAsksNothingTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeAsksNothing"));
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	int32 Asked = 0;
	CountQuestions(*Edited.Toolkit, Asked);

	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));
	TestTrue(TEXT("Bake to Skeletal Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToSkeletalMeshEntry));
	TestEqual(TEXT("Nothing asks about unsaved edits"), Asked, 0);
	TestNotNull(TEXT("The static mesh is written"), Edited.Fixture.FindMesh());
	TestNotNull(TEXT("The skeletal mesh is written"), Edited.Fixture.FindSkeletalMesh());
	return true;
}

// ATS-48: the baked mesh matches the editor's edits; the asset and the edits are unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeEditsTest,
	"Cambriana.Editor.GrammarEditor.Bake.BakeToStaticMeshBakesTheEditorsEditsBesideTheAssetAndLeavesTheAssetAndEditsUnsaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeEditsTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeEdits"));
	const FString TextBefore = Edited.Fixture.Grammar->GrammarText;
	const int32 SidesBefore = Edited.Fixture.Grammar->Sides;
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	int32 Asked = 0;
	CountQuestions(*Edited.Toolkit, Asked);
	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));

	// FBakeFixture: SM_<asset name> in the folder beside the asset, named after it.
	TestEqual(TEXT("The static mesh beside the asset has the edited 6 sides"), RenderedTriangles(Edited.Fixture.FindMesh()), SkTrianglesAtSixSides);
	TestTrue(TEXT("The static mesh is saved to disk"), Edited.Fixture.MeshFileExists());
	TestTrue(TEXT("No skeletal outputs are written"), !Edited.Fixture.FindSkeletalMesh() && !Edited.Fixture.FindSkeleton() && !Edited.Fixture.FindPhysicsAsset());
	TestEqual(TEXT("Nothing asks"), Asked, 0);
	TestAssetAndEditsUnchanged(*this, Edited, TextBefore, SidesBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeEditsSkeletalTest,
	"Cambriana.Editor.GrammarEditor.Bake.BakeToSkeletalMeshBakesTheEditorsEditsBesideTheAssetAndLeavesTheAssetAndEditsUnsaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeEditsSkeletalTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeEditsSkeletal"));
	const FString TextBefore = Edited.Fixture.Grammar->GrammarText;
	const int32 SidesBefore = Edited.Fixture.Grammar->Sides;
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	int32 Asked = 0;
	CountQuestions(*Edited.Toolkit, Asked);
	TestTrue(TEXT("Bake to Skeletal Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToSkeletalMeshEntry));

	TestTrue(TEXT("The skeletal mesh and skeleton are saved to disk"), Edited.Fixture.SkeletalFilesExist());
	TestTrue(TEXT("The physics asset is saved to disk"), Edited.Fixture.FindPhysicsAsset() && Edited.Fixture.PhysicsAssetFileExists());
	TestNull(TEXT("No static mesh is written"), Edited.Fixture.FindMesh());
	if (const USkeletalMesh* Mesh = Edited.Fixture.FindSkeletalMesh())
	{
		const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
		const int32 Triangles = RenderData && RenderData->LODRenderData.Num() > 0 ? int32(RenderData->LODRenderData[0].GetTotalFaces()) : 0;
		TestEqual(TEXT("The skeletal mesh has the edited 6 sides"), Triangles, SkTrianglesAtSixSides);
	}
	else
	{
		AddError(TEXT("The skeletal mesh SK_EditorBakeEditsSkeletal is not written"));
	}
	TestEqual(TEXT("Nothing asks"), Asked, 0);
	TestAssetAndEditsUnchanged(*this, Edited, TextBefore, SidesBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakePendingRulesTest,
	"Cambriana.Editor.GrammarEditor.Bake.BakeAppliesAPendingRulesEditAndBakesIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakePendingRulesTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakePendingRules"));
	const FString TextBefore = Edited.Fixture.Grammar->GrammarText;
	const int32 SidesBefore = Edited.Fixture.Grammar->Sides;
	Edited.Toolkit = OpenGrammarEditor(Edited.Fixture.Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Edited.Toolkit ? RulesTextBoxOf(*Edited.Toolkit) : nullptr;
	if (!TestNotNull(TEXT("The grammar editor shows its Rules text"), Rules.Get()))
	{
		return false;
	}
	TypeOverRules(*Rules);
	int32 Asked = 0;
	CountQuestions(*Edited.Toolkit, Asked);
	TestTrue(TEXT("Bake to Static Mesh runs while the typing has not paused"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));

	TestEqual(TEXT("The static mesh is the typed grammar's two segments"), RenderedTriangles(Edited.Fixture.FindMesh()), TypedTriangles);
	TestEqual(TEXT("Nothing asks"), Asked, 0);
	TestAssetAndEditsUnchanged(*this, Edited, TextBefore, SidesBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeNoEditsTest,
	"Cambriana.Editor.GrammarEditor.Bake.WithoutUnsavedEditsItBakesTheAssetWithoutAsking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeNoEditsTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeNoEdits"));
	Edited.Toolkit = OpenGrammarEditor(Edited.Fixture.Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.Toolkit))
	{
		return false;
	}
	int32 Asked = 0;
	CountQuestions(*Edited.Toolkit, Asked);
	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));
	TestEqual(TEXT("It does not ask"), Asked, 0);
	TestEqual(TEXT("The static mesh is baked"), RenderedTriangles(Edited.Fixture.FindMesh()), SkTriangles);
	return true;
}

#endif
