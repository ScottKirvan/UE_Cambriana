#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CloseGrammarEditor;
using Cambriana::Editor::Tests::CreateGrammarInPackage;
using Cambriana::Editor::Tests::DiscardGrammar;
using Cambriana::Editor::Tests::EditGrammarSides;
using Cambriana::Editor::Tests::FindCategory;
using Cambriana::Editor::Tests::MakeRows;
using Cambriana::Editor::Tests::OpenGrammarEditor;
using Cambriana::Editor::Tests::PackageFilename;
using Cambriana::Editor::Tests::PauseTyping;
using Cambriana::Editor::Tests::RunWithoutSavePrompt;
using Cambriana::Editor::Tests::UnloadPackage;

namespace
{
	const TCHAR* const SourceFileLabel = TEXT("Source File");

	TSharedPtr<SWidget> FindWidget(const TSharedRef<SWidget>& Root, TFunctionRef<bool(const TSharedRef<SWidget>&)> Predicate)
	{
		if (Predicate(Root))
		{
			return Root;
		}
		FChildren* Children = Root->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SWidget> Found = FindWidget(Children->GetChildAt(Index), Predicate))
			{
				return Found;
			}
		}
		return nullptr;
	}

	bool ShowsText(const TSharedPtr<SWidget>& Widget, const FString& Text)
	{
		return Widget && FindWidget(Widget.ToSharedRef(), [&Text](const TSharedRef<SWidget>& Candidate)
		{
			return Candidate->GetType() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Candidate)->GetText().ToString().Equals(Text, ESearchCase::CaseSensitive);
		});
	}

	TSharedPtr<IDetailTreeNode> FindRowLabelled(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, const FString& Label)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			if (Node->GetNodeType() != EDetailNodeType::Category && ShowsText(Node->CreateNodeWidgets().NameWidget, Label))
			{
				return Node;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children);
			if (TSharedPtr<IDetailTreeNode> Found = FindRowLabelled(Children, Label))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** The Source File row the engine's Import Settings display adds (AssetImportDataCustomization.cpp), in the Import Settings category. */
	TSharedPtr<IDetailTreeNode> FindSourceFileRow(const IPropertyRowGenerator& Rows)
	{
		const TSharedPtr<IDetailTreeNode> ImportSettings = FindCategory(Rows, TEXT("ImportSettings"));
		if (!ImportSettings)
		{
			return nullptr;
		}
		TArray<TSharedRef<IDetailTreeNode>> Children;
		ImportSettings->GetChildren(Children);
		return FindRowLabelled(Children, SourceFileLabel);
	}

	/** The path the Source File row's field shows; empty without a row. */
	FString ShownSourceFile(const TSharedPtr<IDetailTreeNode>& Row)
	{
		const TSharedPtr<SWidget> Value = Row ? Row->CreateNodeWidgets().ValueWidget : nullptr;
		if (!Value)
		{
			return FString();
		}
		// Bound Slate attributes refresh in the prepass of each frame, which a widget outside a window never gets.
		Value->SlatePrepass(1.0f);
		const TSharedPtr<SWidget> Field = Cambriana::Editor::Tests::FindWidgetOfType(Value.ToSharedRef(), TEXT("SEditableText"));
		return Field ? StaticCastSharedPtr<SEditableText>(Field)->GetText().ToString() : FString();
	}

	/** Clicks the Source File row's clear button, its button showing an image rather than the "..." text; returns whether the row has one. */
	bool ClickClearSourceFile(const TSharedPtr<IDetailTreeNode>& Row)
	{
		const TSharedPtr<SWidget> Value = Row ? Row->CreateNodeWidgets().ValueWidget : nullptr;
		const TSharedPtr<SWidget> Button = Value ? FindWidget(Value.ToSharedRef(), [](const TSharedRef<SWidget>& Candidate)
		{
			return Candidate->GetType() == TEXT("SButton") && FindWidget(Candidate, [](const TSharedRef<SWidget>& Inner) { return Inner->GetType() == TEXT("SImage"); });
		}) : nullptr;
		if (!Button)
		{
			return false;
		}
		StaticCastSharedPtr<SButton>(Button)->SimulateClick();
		return true;
	}

	FString RecordedSourceFile(const UCambrianaGrammar& Grammar)
	{
		const TArray<FAssetImportInfo::FSourceFile>& Files = Grammar.AssetImportData->GetSourceData().SourceFiles;
		return Files.Num() ? Files[0].RelativeFilename : FString();
	}

	/** A source file on disk and a grammar asset in PackageName that records it as its source file, as an import does. */
	struct FImportedGrammar
	{
		explicit FImportedGrammar(const TCHAR* PackageName)
			: SourcePath(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaImportSettings"), TEXT("tree.lsys"))))
		{
			FFileHelper::SaveStringToFile(FString(TEXT("axiom F\n")), *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Grammar = CreateGrammarInPackage(PackageName);
			Grammar->AssetImportData->Update(SourcePath);
			Grammar->GetPackage()->SetDirtyFlag(false);
		}

		~FImportedGrammar()
		{
			IFileManager::Get().Delete(*SourcePath);
		}

		FImportedGrammar(const FImportedGrammar&) = delete;
		FImportedGrammar& operator=(const FImportedGrammar&) = delete;

		FString SourcePath;
		UCambrianaGrammar* Grammar = nullptr;
	};

	void DeleteSavedPackage(const FString& Filename)
	{
		IFileManager::Get().Delete(*Filename);
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Filename));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsShownTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.DetailsShowsImportSettingsSourceFileWithTheImportedFilesPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsShownTest::RunTest(const FString& Parameters)
{
	FImportedGrammar Imported(TEXT("/Game/CambrianaEditorTests/ImportSettingsShown"));
	UCambrianaGrammar* Grammar = Imported.Grammar;
	TestTrue(TEXT("The asset records the imported file as its source file"), FPaths::IsSamePath(Grammar->AssetImportData->GetFirstFilename(), Imported.SourcePath));

	const TSharedRef<IPropertyRowGenerator> AssetRows = MakeRows({ Grammar });
	const FString AssetShown = ShownSourceFile(FindSourceFileRow(*AssetRows));
	TestTrue(TEXT("The asset's own Details shows Import Settings > Source File with the imported file's path"),
		!AssetShown.IsEmpty() && FPaths::IsSamePath(UAssetImportData::ResolveImportFilename(AssetShown, Grammar->GetPackage()), Imported.SourcePath));

	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	TestTrue(TEXT("The grammar editor's Details shows the working copy's import data"),
		Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, AssetImportData)));
	TestTrue(TEXT("The grammar editor's Details shows the import data's source file"),
		Toolkit->IsDetailsPropertyVisible(TEXT("SourceData"), { GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, AssetImportData) }));
	TestFalse(TEXT("The grammar editor's Details still hides the grammar text"),
		Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText)));
	TestTrue(TEXT("The grammar editor's Details still shows the size"), Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, SizeValue)));

	const TSharedRef<IPropertyRowGenerator> WorkingRows = MakeRows({ Toolkit->GetWorkingCopy() });
	const TSharedPtr<IDetailTreeNode> Row = FindSourceFileRow(*WorkingRows);
	TestTrue(TEXT("The working copy's Details has Import Settings > Source File"), Row.IsValid());
	const FString Shown = ShownSourceFile(Row);
	TestEqualSensitive(TEXT("Source File shows the path the asset records"), Shown, RecordedSourceFile(*Grammar));
	TestTrue(TEXT("Source File shows the imported file's path"),
		!Shown.IsEmpty() && FPaths::IsSamePath(UAssetImportData::ResolveImportFilename(Shown, Grammar->GetPackage()), Imported.SourcePath));
	TestFalse(TEXT("Opening the editor leaves nothing unsaved"), Toolkit->HasUnsavedEdits());
	DiscardGrammar(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsClearSaveTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.ClearingSourceFileLeavesTheAssetUnchangedUntilSaveWhichClearsTheAssetsOwnImportData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsClearSaveTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/ImportSettingsCleared");
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);
	FImportedGrammar Imported(PackageName);
	UCambrianaGrammar* Grammar = Imported.Grammar;
	UPackage* Package = Grammar->GetPackage();
	UAssetImportData* AssetImportData = Grammar->AssetImportData;
	const FString Recorded = RecordedSourceFile(*Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}

	const TSharedRef<IPropertyRowGenerator> WorkingRows = MakeRows({ Toolkit->GetWorkingCopy() });
	if (!TestTrue(TEXT("Source File has a clear button"), ClickClearSourceFile(FindSourceFileRow(*WorkingRows))))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	TestTrue(TEXT("The working copy's source file is cleared"), RecordedSourceFile(*Toolkit->GetWorkingCopy()).IsEmpty());
	TestEqualSensitive(TEXT("The asset's source file is unchanged"), RecordedSourceFile(*Grammar), Recorded);
	TestFalse(TEXT("The asset's package stays clean"), Package->IsDirty());
	TestTrue(TEXT("The cleared source file is an unsaved edit"), Toolkit->HasUnsavedEdits());
	TestEqual(TEXT("The tab marks the unsaved edit"), Toolkit->GetTabSuffix().ToString(), FString(TEXT("*")));

	RunWithoutSavePrompt([Toolkit] { Toolkit->SaveAsset_Execute(); });
	TestTrue(TEXT("After Save the asset's source file is cleared"), RecordedSourceFile(*Grammar).IsEmpty());
	TestTrue(TEXT("The asset keeps its own import data object"), Grammar->AssetImportData == AssetImportData);
	TestTrue(TEXT("The asset's import data is not the working copy's"), Grammar->AssetImportData != Toolkit->GetWorkingCopy()->AssetImportData);
	TestTrue(TEXT("The asset's import data is still the asset's subobject"), Grammar->AssetImportData->GetOuter() == Grammar);
	TestFalse(TEXT("Nothing is left unsaved"), Toolkit->HasUnsavedEdits());
	TestTrue(TEXT("The tab shows no unsaved mark"), Toolkit->GetTabSuffix().IsEmpty());

	CloseGrammarEditor(Grammar);
	TestTrue(TEXT("The package unloads"), UnloadPackage(Package));
	UPackage* LoadedPackage = IFileManager::Get().FileExists(*Filename) ? LoadPackage(nullptr, PackageName, LOAD_None) : nullptr;
	const UCambrianaGrammar* Loaded = LoadedPackage ? FindObject<UCambrianaGrammar>(LoadedPackage, *FPackageName::GetShortName(PackageName)) : nullptr;
	if (TestNotNull(TEXT("The saved grammar loads again"), Loaded))
	{
		TestTrue(TEXT("The loaded grammar's source file is cleared"), RecordedSourceFile(*Loaded).IsEmpty());
	}
	if (LoadedPackage)
	{
		UnloadPackage(LoadedPackage);
	}
	DeleteSavedPackage(Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsDiscardTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.DiscardingAClearedSourceFileKeepsTheAssetsSourceFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsDiscardTest::RunTest(const FString& Parameters)
{
	FImportedGrammar Imported(TEXT("/Game/CambrianaEditorTests/ImportSettingsDiscarded"));
	UCambrianaGrammar* Grammar = Imported.Grammar;
	const FString Recorded = RecordedSourceFile(*Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	{
		const TSharedRef<IPropertyRowGenerator> WorkingRows = MakeRows({ Toolkit->GetWorkingCopy() });
		TestTrue(TEXT("Source File has a clear button"), ClickClearSourceFile(FindSourceFileRow(*WorkingRows)));
	}
	TestTrue(TEXT("The cleared source file is an unsaved edit"), Toolkit->HasUnsavedEdits());

	CloseGrammarEditor(Grammar);
	TestNull(TEXT("The editor closed"), Cambriana::Editor::Tests::FindGrammarEditor(Grammar));
	TestEqualSensitive(TEXT("Discarding keeps the asset's source file"), RecordedSourceFile(*Grammar), Recorded);
	TestTrue(TEXT("The asset can still reimport from the imported file"), FPaths::IsSamePath(Grammar->AssetImportData->GetFirstFilename(), Imported.SourcePath));
	UnloadPackage(Grammar->GetPackage());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsUndoTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.UndoRestoresAClearedSourceFileAndRedoClearsItAgain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsUndoTest::RunTest(const FString& Parameters)
{
	FImportedGrammar Imported(TEXT("/Game/CambrianaEditorTests/ImportSettingsUndone"));
	UCambrianaGrammar* Grammar = Imported.Grammar;
	const FString Recorded = RecordedSourceFile(*Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> WorkingRows = MakeRows({ Toolkit->GetWorkingCopy() });
	if (!TestTrue(TEXT("Source File has a clear button"), ClickClearSourceFile(FindSourceFileRow(*WorkingRows))))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	TestTrue(TEXT("The working copy's source file is cleared"), RecordedSourceFile(*Toolkit->GetWorkingCopy()).IsEmpty());

	TestTrue(TEXT("The clear can be undone"), GEditor->UndoTransaction());
	TestEqualSensitive(TEXT("Undo restores the working copy's source file"), RecordedSourceFile(*Toolkit->GetWorkingCopy()), Recorded);
	TestEqualSensitive(TEXT("Source File shows the restored path"), ShownSourceFile(FindSourceFileRow(*WorkingRows)), Recorded);
	TestFalse(TEXT("Undoing back to the asset's source file leaves nothing unsaved"), Toolkit->HasUnsavedEdits());
	TestTrue(TEXT("The tab shows no unsaved mark"), Toolkit->GetTabSuffix().IsEmpty());

	TestTrue(TEXT("The clear can be redone"), GEditor->RedoTransaction());
	TestTrue(TEXT("Redo clears the working copy's source file again"), RecordedSourceFile(*Toolkit->GetWorkingCopy()).IsEmpty());
	TestEqual(TEXT("The tab marks the unsaved edit again"), Toolkit->GetTabSuffix().ToString(), FString(TEXT("*")));
	TestEqualSensitive(TEXT("The asset's source file stays unchanged"), RecordedSourceFile(*Grammar), Recorded);
	DiscardGrammar(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsKeptTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.AnAssetChangeAfterSourceFileIsClearedKeepsTheClearedWorkingCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsKeptTest::RunTest(const FString& Parameters)
{
	FImportedGrammar Imported(TEXT("/Game/CambrianaEditorTests/ImportSettingsKept"));
	UCambrianaGrammar* Grammar = Imported.Grammar;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	{
		const TSharedRef<IPropertyRowGenerator> WorkingRows = MakeRows({ Toolkit->GetWorkingCopy() });
		TestTrue(TEXT("Source File has a clear button"), ClickClearSourceFile(FindSourceFileRow(*WorkingRows)));
	}

	EditGrammarSides(Grammar, 3);
	TestTrue(TEXT("The working copy keeps its cleared source file"), RecordedSourceFile(*Toolkit->GetWorkingCopy()).IsEmpty());
	TestEqual(TEXT("With an unsaved edit the working copy keeps its own sides"), Toolkit->GetWorkingCopy()->Sides, 8);
	TestTrue(TEXT("The edit is still unsaved"), Toolkit->HasUnsavedEdits());
	DiscardGrammar(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsFollowTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.ClearingTheAssetsOwnSourceFileReachesAWorkingCopyWithoutEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsFollowTest::RunTest(const FString& Parameters)
{
	FImportedGrammar Imported(TEXT("/Game/CambrianaEditorTests/ImportSettingsFollowed"));
	UCambrianaGrammar* Grammar = Imported.Grammar;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	{
		const TSharedRef<IPropertyRowGenerator> AssetRows = MakeRows({ Grammar });
		TestTrue(TEXT("The asset's own Source File has a clear button"), ClickClearSourceFile(FindSourceFileRow(*AssetRows)));
	}
	TestTrue(TEXT("The asset's source file is cleared"), RecordedSourceFile(*Grammar).IsEmpty());
	PauseTyping(0.0f);
	TestTrue(TEXT("The working copy takes the asset's cleared source file"), RecordedSourceFile(*Toolkit->GetWorkingCopy()).IsEmpty());
	TestFalse(TEXT("The editor has no unsaved edits"), Toolkit->HasUnsavedEdits());
	DiscardGrammar(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsRelativePathTest,
	"Cambriana.Editor.GrammarEditor.ImportSettings.ASourceFileRecordedRelativeToTheAssetsPackageResolvesToTheImportedFileInTheWorkingCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsRelativePathTest::RunTest(const FString& Parameters)
{
	FImportedGrammar Imported(TEXT("/Game/CambrianaEditorTests/ImportSettingsRelative"));
	UCambrianaGrammar* Grammar = Imported.Grammar;
	// A file under the project directory is recorded relative to a /Game package's file (UAssetImportData::SanitizeImportFilename).
	TestTrue(TEXT("The asset records its source file relative to its package"), FPaths::IsRelative(RecordedSourceFile(*Grammar)));
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	TestTrue(TEXT("The working copy's source file resolves to the imported file"),
		FPaths::IsSamePath(Toolkit->GetWorkingCopy()->AssetImportData->GetFirstFilename(), Imported.SourcePath));
	TArray<FString> Filenames;
	Toolkit->GetWorkingCopy()->AssetImportData->ExtractFilenames(Filenames);
	TestTrue(TEXT("The working copy's source files resolve to the imported file"), Filenames.Num() == 1 && FPaths::IsSamePath(Filenames[0], Imported.SourcePath));
	DiscardGrammar(Grammar);
	return true;
}

#endif
