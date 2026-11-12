#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaLiveActor.h"
#include "Editor.h"
#include "Containers/Ticker.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::PlaceInCurrentLevel;

namespace
{
	UCambrianaGrammar* CreateGrammarWithFactory()
	{
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		return Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass()), RF_Transactional, nullptr, nullptr));
	}

	FCambrianaGrammarEditorToolkit* OpenGrammarEditor(UCambrianaGrammar* Grammar)
	{
		UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (!AssetEditors->OpenEditorForAsset(Grammar))
		{
			return nullptr;
		}
		IAssetEditorInstance* Editor = AssetEditors->FindEditorForAsset(Grammar, false);
		return Editor && Editor->GetEditorName() == FCambrianaGrammarEditorToolkit::ToolkitName
			? static_cast<FCambrianaGrammarEditorToolkit*>(Editor)
			: nullptr;
	}

	void CloseGrammarEditor(UCambrianaGrammar* Grammar)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Grammar);
	}

	TSharedPtr<SMultiLineEditableTextBox> RulesTextBoxOf(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		const TSharedPtr<SDockTab> Tab = Toolkit.GetTabManager()->FindExistingLiveTab(FTabId(FCambrianaGrammarEditorToolkit::RulesTabId));
		return Tab ? StaticCastSharedRef<SMultiLineEditableTextBox>(Tab->GetContent()) : TSharedPtr<SMultiLineEditableTextBox>();
	}

	const TCHAR* const TypedGrammarText = TEXT("generations 2\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	void TypeOverRules(SMultiLineEditableTextBox& Rules)
	{
		Rules.SelectAllText();
		Rules.InsertTextAtCursor(FString(TEXT("generations 2\naxiom A\n\ngrow {\n")));
		Rules.InsertTextAtCursor(FString(TEXT("    A -> F A\n")));
		Rules.InsertTextAtCursor(FString(TEXT("}\n")));
	}

	void PauseTyping(float Seconds)
	{
		FTSTicker::GetCoreTicker().Tick(Seconds);
	}

	UCambrianaGrammar* CreateGrammarInPackage(const TCHAR* PackageName)
	{
		UPackage* Package = CreatePackage(PackageName);
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		return Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(), Package,
			*FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional, nullptr, nullptr));
	}

	bool UnloadPackage(UPackage* Package)
	{
		FText Error;
		return UPackageTools::UnloadPackages({ Package }, Error, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorOpensWithTabsTest,
	"Cambriana.Editor.GrammarEditor.OpeningAGrammarOpensTheGrammarEditorWithRulesAndDetailsTabs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorOpensWithTabsTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestTrue(TEXT("The editor edits the grammar"), Toolkit->GetEditingObject() == Grammar);
	const TSharedPtr<FTabManager> Tabs = Toolkit->GetTabManager();
	if (TestTrue(TEXT("The editor has a tab manager"), Tabs.IsValid()))
	{
		TestTrue(TEXT("A Rules tab is registered"), Tabs->HasTabSpawner(FCambrianaGrammarEditorToolkit::RulesTabId));
		TestTrue(TEXT("A Details tab is registered"), Tabs->HasTabSpawner(FCambrianaGrammarEditorToolkit::DetailsTabId));
		TestTrue(TEXT("The Rules tab is open"), Tabs->FindExistingLiveTab(FCambrianaGrammarEditorToolkit::RulesTabId).IsValid());
		TestTrue(TEXT("The Details tab is open"), Tabs->FindExistingLiveTab(FCambrianaGrammarEditorToolkit::DetailsTabId).IsValid());
	}
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorRulesShowTextTest,
	"Cambriana.Editor.GrammarEditor.RulesTabShowsTheGrammarText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorRulesShowTextTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TEXT("axiom X\n\ngrow {\n    X -> F [ + X ] - X\n}\n");
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestEqual(TEXT("Rules text"), Toolkit->GetRulesText().ToString(), FString(TEXT("axiom X\n\ngrow {\n    X -> F [ + X ] - X\n}\n")));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorRulesEditTest,
	"Cambriana.Editor.GrammarEditor.RulesEditUpdatesTheGrammarRebuildsItsActorsAndUndoes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorRulesEditTest::RunTest(const FString& Parameters)
{
	const FString OldText = UCambrianaGrammarFactory::DefaultGrammarText;
	const FString NewText = TEXT("generations 2\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		if (Actor)
		{
			Actor->GetWorld()->EditorDestroyActor(Actor, false);
		}
		return false;
	}

	const int32 BuildsBeforeEdit = Actor->GetBuildCount();
	Toolkit->CommitRulesText(FText::FromString(NewText));
	TestEqual(TEXT("The grammar holds the edited text"), Grammar->GrammarText, NewText);
	TestEqual(TEXT("The actor rebuilt once"), Actor->GetBuildCount(), BuildsBeforeEdit + 1);
	TestEqual(TEXT("The actor built the edited text"), Actor->GetLastBuildInput().GrammarText, NewText);

	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores the grammar's text"), Grammar->GrammarText, OldText);
	TestEqual(TEXT("Undo restores the Rules text"), Toolkit->GetRulesText().ToString(), OldText);
	TestEqual(TEXT("The actor built the restored text"), Actor->GetLastBuildInput().GrammarText, OldText);

	CloseGrammarEditor(Grammar);
	Actor->GetWorld()->EditorDestroyActor(Actor, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorNewAssetTest,
	"Cambriana.Editor.GrammarEditor.NewGrammarOpensInTheGrammarEditorWithDefaultGrammarAndSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorNewAssetTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A new grammar opens its editor after creation"), GetDefault<UCambrianaGrammarFactory>()->bEditAfterNew);

	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The new grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestEqual(TEXT("Rules shows the default grammar"), Toolkit->GetRulesText().ToString(), FString(UCambrianaGrammarFactory::DefaultGrammarText));
	const TArray<TWeakObjectPtr<UObject>> DetailsObjects = Toolkit->GetDetailsObjects();
	TestTrue(TEXT("Details shows the new grammar"), DetailsObjects.Num() == 1 && DetailsObjects[0].Get() == Grammar);
	TestTrue(TEXT("Details shows the seed"), Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed)));
	TestFalse(TEXT("Details leaves the grammar text to Rules"),
		Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText)));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingDirtiesTheGrammarAndReachesItAfterAPauseAsOneRebuildAndOneUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorTypingTest::RunTest(const FString& Parameters)
{
	const FString OldText = UCambrianaGrammarFactory::DefaultGrammarText;
	UCambrianaGrammar* Grammar = CreateGrammarInPackage(TEXT("/Game/CambrianaEditorTests/TypedGrammar"));
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		if (Actor)
		{
			Actor->GetWorld()->EditorDestroyActor(Actor, false);
		}
		UnloadPackage(Grammar->GetPackage());
		return false;
	}
	Grammar->GetPackage()->SetDirtyFlag(false);
	const int32 BuildsBeforeTyping = Actor->GetBuildCount();

	TypeOverRules(*Rules);
	TestTrue(TEXT("Typing marks the grammar's package dirty at once"), Grammar->GetPackage()->IsDirty());

	PauseTyping(1.0f);
	TestEqual(TEXT("After a one-second pause the grammar holds the typed text"), Grammar->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("The actor rebuilt once for the whole typed edit"), Actor->GetBuildCount(), BuildsBeforeTyping + 1);
	TestEqual(TEXT("The actor built the typed text"), Actor->GetLastBuildInput().GrammarText, FString(TypedGrammarText));

	TestTrue(TEXT("The typed edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("One undo restores the grammar's text from before typing"), Grammar->GrammarText, OldText);
	TestEqual(TEXT("Undo restores the Rules text"), Toolkit->GetRulesText().ToString(), OldText);

	CloseGrammarEditor(Grammar);
	Actor->GetWorld()->EditorDestroyActor(Actor, false);
	UnloadPackage(Grammar->GetPackage());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingSavedTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingIsSavedWhenTheGrammarIsSavedBeforeAPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorTypingSavedTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/TypedGrammar");
	const FString ShortName = FPackageName::GetShortName(PackageName);
	const FString PackageFilename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()));
	IFileManager::Get().Delete(*PackageFilename);

	UCambrianaGrammar* Grammar = CreateGrammarInPackage(PackageName);
	UPackage* Package = Grammar->GetPackage();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		UnloadPackage(Package);
		return false;
	}

	TypeOverRules(*Rules);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	const bool bSaved = UPackage::SavePackage(Package, Grammar, *PackageFilename, SaveArgs);
	TestTrue(TEXT("The package saves to disk"), bSaved);

	CloseGrammarEditor(Grammar);
	TestTrue(TEXT("The package unloads"), UnloadPackage(Package));
	Grammar = nullptr;
	Package = nullptr;

	UPackage* LoadedPackage = bSaved ? LoadPackage(nullptr, PackageName, LOAD_None) : nullptr;
	const UCambrianaGrammar* Loaded = LoadedPackage ? FindObject<UCambrianaGrammar>(LoadedPackage, *ShortName) : nullptr;
	if (TestNotNull(TEXT("The saved grammar loads again"), Loaded))
	{
		TestEqual(TEXT("The saved grammar holds the typed text"), Loaded->GrammarText, FString(TypedGrammarText));
	}
	if (LoadedPackage)
	{
		UnloadPackage(LoadedPackage);
	}
	IFileManager::Get().Delete(*PackageFilename);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingClosedTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingReachesTheGrammarWhenTheEditorClosesBeforeAPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorTypingClosedTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TypeOverRules(*Rules);
	CloseGrammarEditor(Grammar);
	TestEqual(TEXT("Closing the editor gives the grammar the typed text"), Grammar->GrammarText, FString(TypedGrammarText));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorDelegatesTest,
	"Cambriana.Editor.GrammarEditor.WidgetCallbacksDoNotRunOnceTheEditorIsGone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorDelegatesTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FCambrianaGrammarEditorToolkit> Toolkit = MakeShared<FCambrianaGrammarEditorToolkit>();
	const FOnTextCommitted RulesCommitted = Toolkit->MakeRulesCommittedDelegate();
	const FIsPropertyVisible DetailsPropertyVisible = Toolkit->MakeDetailsPropertyVisibleDelegate();
	TestTrue(TEXT("The Rules commit callback is bound while the editor exists"), RulesCommitted.IsBound());
	TestTrue(TEXT("The Details visibility callback is bound while the editor exists"), DetailsPropertyVisible.IsBound());

	Toolkit.Reset();
	TestFalse(TEXT("The Rules commit callback is unbound once the editor is gone"), RulesCommitted.IsBound());
	TestFalse(TEXT("The Details visibility callback is unbound once the editor is gone"), DetailsPropertyVisible.IsBound());
	return true;
}

#endif
