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

// ATS-48: with unsaved edits, either choice asks to save.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeAsksToSaveTest,
	"Cambriana.Editor.GrammarEditor.Bake.WithUnsavedEditsEitherChoiceAsksToSaveOnceWithYesAndNo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeAsksToSaveTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeAsks"));
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	int32 Asked = 0;
	TArray<EAppMsgType::Type> Offered;
	TArray<FString> Messages;
	Edited.Toolkit->SetUnsavedEditsPrompt([&](EAppMsgType::Type Buttons, const FText& Message)
	{
		++Asked;
		Offered.Add(Buttons);
		Messages.Add(Message.ToString());
		return EAppReturnType::No;
	});

	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));
	TestEqual(TEXT("It asks once"), Asked, 1);
	TestTrue(TEXT("Bake to Skeletal Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToSkeletalMeshEntry));
	TestEqual(TEXT("It asks once more"), Asked, 2);
	for (int32 Index = 0; Index < Offered.Num(); ++Index)
	{
		TestTrue(TEXT("The question offers Yes and No"), Offered[Index] == EAppMsgType::YesNo);
		TestTrue(TEXT("The question names the grammar"), Messages[Index].Contains(Edited.Fixture.Grammar->GetPathName()));
	}
	return true;
}

// ATS-48: on save, the baked mesh matches the saved grammar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeSaveThenBakeTest,
	"Cambriana.Editor.GrammarEditor.Bake.SavingThenBakingToStaticMeshBakesTheSavedGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeSaveThenBakeTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeSaves"));
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	Edited.Toolkit->SetUnsavedEditsPrompt([](EAppMsgType::Type, const FText&) { return EAppReturnType::Yes; });
	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));

	TestEqual(TEXT("The asset holds the edited sides"), Edited.Fixture.Grammar->Sides, EditedSides);
	TestTrue(TEXT("The asset is saved to disk"), IFileManager::Get().FileExists(*PackageFilename(*Edited.Fixture.GrammarPackage)));
	TestFalse(TEXT("The editor has nothing unsaved"), Edited.Toolkit->HasUnsavedEdits());
	TestEqual(TEXT("The static mesh is the saved grammar's, not the grammar as it was"), RenderedTriangles(Edited.Fixture.FindMesh()), SkTrianglesAtSixSides);
	TestTrue(TEXT("The static mesh is saved to disk"), Edited.Fixture.MeshFileExists());
	TestTrue(TEXT("No skeletal outputs are written"), !Edited.Fixture.FindSkeletalMesh() && !Edited.Fixture.FindSkeleton() && !Edited.Fixture.FindPhysicsAsset());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeSaveThenBakeSkeletalTest,
	"Cambriana.Editor.GrammarEditor.Bake.SavingThenBakingToSkeletalMeshWritesTheSkeletalOutputsFromTheSavedGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeSaveThenBakeSkeletalTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeSavesSkeletal"));
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	Edited.Toolkit->SetUnsavedEditsPrompt([](EAppMsgType::Type, const FText&) { return EAppReturnType::Yes; });
	TestTrue(TEXT("Bake to Skeletal Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToSkeletalMeshEntry));

	TestEqual(TEXT("The asset holds the edited sides"), Edited.Fixture.Grammar->Sides, EditedSides);
	TestTrue(TEXT("The skeletal mesh and skeleton are saved to disk"), Edited.Fixture.SkeletalFilesExist());
	TestTrue(TEXT("The physics asset is saved to disk"), Edited.Fixture.FindPhysicsAsset() && Edited.Fixture.PhysicsAssetFileExists());
	TestNull(TEXT("No static mesh is written"), Edited.Fixture.FindMesh());
	if (const USkeletalMesh* Mesh = Edited.Fixture.FindSkeletalMesh())
	{
		const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
		const int32 Triangles = RenderData && RenderData->LODRenderData.Num() > 0 ? int32(RenderData->LODRenderData[0].GetTotalFaces()) : 0;
		TestEqual(TEXT("The skeletal mesh is the saved grammar's"), Triangles, SkTrianglesAtSixSides);
	}
	else
	{
		AddError(TEXT("The skeletal mesh SK_EditorBakeSavesSkeletal is not written"));
	}
	return true;
}

// ATS-48: on cancel, nothing is baked and the asset is unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeCancelTest,
	"Cambriana.Editor.GrammarEditor.Bake.DecliningTheSaveBakesNothingAndLeavesTheAssetUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeCancelTest::RunTest(const FString& Parameters)
{
	FEditedGrammar Edited(TEXT("EditorBakeCancels"));
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Edited.OpenAndEdit()))
	{
		return false;
	}
	const int32 SidesBefore = Edited.Fixture.Grammar->Sides;
	Edited.Toolkit->SetUnsavedEditsPrompt([](EAppMsgType::Type, const FText&) { return EAppReturnType::No; });
	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));
	TestTrue(TEXT("Bake to Skeletal Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToSkeletalMeshEntry));

	TestNull(TEXT("No static mesh"), Edited.Fixture.FindMesh());
	TestTrue(TEXT("No skeletal mesh, skeleton or physics asset"), !Edited.Fixture.FindSkeletalMesh() && !Edited.Fixture.FindSkeleton() && !Edited.Fixture.FindPhysicsAsset());
	TestFalse(TEXT("No output file"), Edited.Fixture.MeshFileExists() || Edited.Fixture.AnySkeletalFileExists() || Edited.Fixture.PhysicsAssetFileExists());
	TestFalse(TEXT("The asset is not saved"), IFileManager::Get().FileExists(*PackageFilename(*Edited.Fixture.GrammarPackage)));
	TestEqual(TEXT("The asset is unchanged"), Edited.Fixture.Grammar->Sides, SidesBefore);
	TestTrue(TEXT("The edits stay in the editor"), Edited.Toolkit->HasUnsavedEdits());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeNoEditsTest,
	"Cambriana.Editor.GrammarEditor.Bake.WithoutUnsavedEditsItBakesStraightAwayWithoutAsking",
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
	Edited.Toolkit->SetUnsavedEditsPrompt([&Asked](EAppMsgType::Type, const FText&) { ++Asked; return EAppReturnType::No; });
	TestTrue(TEXT("Bake to Static Mesh runs"), RunBakeEntry(*Edited.Toolkit, BakeToStaticMeshEntry));
	TestEqual(TEXT("It does not ask"), Asked, 0);
	TestEqual(TEXT("The static mesh is baked"), RenderedTriangles(Edited.Fixture.FindMesh()), SkTriangles);
	return true;
}

#endif
