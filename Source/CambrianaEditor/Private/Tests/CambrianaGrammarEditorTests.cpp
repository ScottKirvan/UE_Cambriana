#include "AdvancedPreviewScene.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarPreview.h"
#include "CambrianaLiveActor.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "IMessageLogListing.h"
#include "IPropertyRowGenerator.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "PropertyEditorModule.h"
#include "SEditorViewport.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CloseAsking;
using Cambriana::Editor::Tests::CloseGrammarEditor;
using Cambriana::Editor::Tests::CreateGrammarInPackage;
using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::EditGrammarSides;
using Cambriana::Editor::Tests::FindWidgetOfType;
using Cambriana::Editor::Tests::OpenGrammarEditor;
using Cambriana::Editor::Tests::PackageFilename;
using Cambriana::Editor::Tests::PauseTyping;
using Cambriana::Editor::Tests::PlaceInCurrentLevel;
using Cambriana::Editor::Tests::RulesTabWidgetOfType;
using Cambriana::Editor::Tests::RulesTextBoxOf;
using Cambriana::Editor::Tests::RunWithoutSavePrompt;
using Cambriana::Editor::Tests::TubeVertices;
using Cambriana::Editor::Tests::TypedGrammarText;
using Cambriana::Editor::Tests::TypeOverRules;
using Cambriana::Editor::Tests::UnloadPackage;
using Cambriana::Editor::Tests::MakeRows;
using Cambriana::Editor::Tests::DestroyActor;
using Cambriana::Editor::Tests::DiscardGrammar;
using Cambriana::Editor::Tests::DiscardGrammarAndActor;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorOpensWithTabsTest,
	"Cambriana.Editor.GrammarEditor.OpeningAGrammarOpensTheGrammarEditorWithRulesViewportDetailsAndOutputsTabs",
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
		TestTrue(TEXT("A Viewport tab is registered"), Tabs->HasTabSpawner(FCambrianaGrammarEditorToolkit::ViewportTabId));
		TestTrue(TEXT("A Details tab is registered"), Tabs->HasTabSpawner(FCambrianaGrammarEditorToolkit::DetailsTabId));
		TestTrue(TEXT("An Outputs tab is registered"), Tabs->HasTabSpawner(FCambrianaGrammarEditorToolkit::OutputsTabId));
		TestTrue(TEXT("The Rules tab is open"), Tabs->FindExistingLiveTab(FCambrianaGrammarEditorToolkit::RulesTabId).IsValid());
		TestTrue(TEXT("The Viewport tab is open"), Tabs->FindExistingLiveTab(FCambrianaGrammarEditorToolkit::ViewportTabId).IsValid());
		TestTrue(TEXT("The Details tab is open"), Tabs->FindExistingLiveTab(FCambrianaGrammarEditorToolkit::DetailsTabId).IsValid());
		TestTrue(TEXT("The Outputs tab is open"), Tabs->FindExistingLiveTab(FCambrianaGrammarEditorToolkit::OutputsTabId).IsValid());
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
	TestEqualSensitive(TEXT("Rules text"), Toolkit->GetRulesText().ToString(), FString(TEXT("axiom X\n\ngrow {\n    X -> F [ + X ] - X\n}\n")));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorRulesEditTest,
	"Cambriana.Editor.GrammarEditor.RulesEditChangesTheWorkingCopyNotTheGrammarOrItsActorsAndUndoes",
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
		DestroyActor(Actor);
		return false;
	}

	const int32 BuildsBeforeEdit = Actor->GetBuildCount();
	Toolkit->CommitRulesText(FText::FromString(NewText));
	TestEqualSensitive(TEXT("The working copy holds the edited text"), Toolkit->GetWorkingCopy()->GrammarText, NewText);
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, OldText);
	TestEqual(TEXT("The actor did not rebuild"), Actor->GetBuildCount(), BuildsBeforeEdit);
	TestEqualSensitive(TEXT("The actor still built the grammar's text"), Actor->GetLastBuildInput().GrammarText, OldText);

	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestEqualSensitive(TEXT("Undo restores the working copy's text"), Toolkit->GetWorkingCopy()->GrammarText, OldText);
	TestEqualSensitive(TEXT("Undo restores the Rules text"), Toolkit->GetRulesText().ToString(), OldText);
	TestEqual(TEXT("The actor did not rebuild for the undo"), Actor->GetBuildCount(), BuildsBeforeEdit);

	CloseGrammarEditor(Grammar);
	DestroyActor(Actor);
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
	TestEqualSensitive(TEXT("Rules shows the default grammar"), Toolkit->GetRulesText().ToString(), FString(UCambrianaGrammarFactory::DefaultGrammarText));
	const TArray<TWeakObjectPtr<UObject>> DetailsObjects = Toolkit->GetDetailsObjects();
	TestTrue(TEXT("Details shows the new grammar's working copy"), DetailsObjects.Num() == 1 && DetailsObjects[0].Get() == Toolkit->GetWorkingCopy());
	TestEqual(TEXT("The working copy holds the new grammar's seed"), Toolkit->GetWorkingCopy()->Seed, Grammar->Seed);
	TestTrue(TEXT("Details shows the seed"), Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed)));
	TestFalse(TEXT("Details leaves the grammar text to Rules"),
		Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText)));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingReachesTheWorkingCopyAfterAPauseAsOneUndoAndLeavesTheGrammarAndItsActorsAlone",
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
		DiscardGrammarAndActor(Grammar, Actor);
		return false;
	}
	Grammar->GetPackage()->SetDirtyFlag(false);
	const int32 BuildsBeforeTyping = Actor->GetBuildCount();
	const int32 PreviewBuildsBeforeTyping = Toolkit->GetPreview().GetBuildCount();

	TypeOverRules(*Rules);
	TestFalse(TEXT("Typing leaves the grammar's package clean"), Grammar->GetPackage()->IsDirty());
	TestTrue(TEXT("Typing is an unsaved edit at once"), Toolkit->HasUnsavedEdits());

	PauseTyping(1.0f);
	TestEqualSensitive(TEXT("After a one-second pause the working copy holds the typed text"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("The preview rebuilt once for the whole typed edit"), Toolkit->GetPreview().GetBuildCount(), PreviewBuildsBeforeTyping + 1);
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, OldText);
	TestFalse(TEXT("The grammar's package stays clean"), Grammar->GetPackage()->IsDirty());
	TestEqual(TEXT("The actor did not rebuild"), Actor->GetBuildCount(), BuildsBeforeTyping);

	TestTrue(TEXT("The typed edit can be undone"), GEditor->UndoTransaction());
	TestEqualSensitive(TEXT("One undo restores the working copy's text from before typing"), Toolkit->GetWorkingCopy()->GrammarText, OldText);
	TestEqualSensitive(TEXT("Undo restores the Rules text"), Toolkit->GetRulesText().ToString(), OldText);
	TestFalse(TEXT("Undoing back to the grammar's text leaves no unsaved edit"), Toolkit->HasUnsavedEdits());

	DiscardGrammarAndActor(Grammar, Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingSavedTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingIsSavedByTheEditorsSaveBeforeAPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorTypingSavedTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/TypedGrammar");
	const FString ShortName = FPackageName::GetShortName(PackageName);
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);

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
	RunWithoutSavePrompt([Toolkit] { Toolkit->SaveAsset_Execute(); });
	TestEqualSensitive(TEXT("The save gives the grammar the typed text"), Grammar->GrammarText, FString(TypedGrammarText));
	TestTrue(TEXT("The package is on disk"), IFileManager::Get().FileExists(*Filename));
	TestFalse(TEXT("The saved package is clean"), Package->IsDirty());
	TestFalse(TEXT("Nothing is left unsaved"), Toolkit->HasUnsavedEdits());

	CloseGrammarEditor(Grammar);
	TestTrue(TEXT("The package unloads"), UnloadPackage(Package));
	Grammar = nullptr;
	Package = nullptr;

	UPackage* LoadedPackage = IFileManager::Get().FileExists(*Filename) ? LoadPackage(nullptr, PackageName, LOAD_None) : nullptr;
	const UCambrianaGrammar* Loaded = LoadedPackage ? FindObject<UCambrianaGrammar>(LoadedPackage, *ShortName) : nullptr;
	if (TestNotNull(TEXT("The saved grammar loads again"), Loaded))
	{
		TestEqualSensitive(TEXT("The saved grammar holds the typed text"), Loaded->GrammarText, FString(TypedGrammarText));
	}
	if (LoadedPackage)
	{
		UnloadPackage(LoadedPackage);
	}
	IFileManager::Get().Delete(*Filename);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Filename));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingClosedTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingBeforeAPauseIsAnUnsavedEditWhenTheEditorCloses",
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
	int32 Asked = 0;
	Toolkit->SetUnsavedEditsPrompt([&Asked](EAppMsgType::Type, const FText&) { ++Asked; return EAppReturnType::No; });
	CloseAsking(Grammar);
	TestEqual(TEXT("Closing asks once about the typed edit"), Asked, 1);
	TestNull(TEXT("Discarding closes the editor"), Cambriana::Editor::Tests::FindGrammarEditor(Grammar));
	TestEqualSensitive(TEXT("The discarded typing leaves the grammar's text"), Grammar->GrammarText, FString(UCambrianaGrammarFactory::DefaultGrammarText));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingSelectionTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingKeepsTheRulesSelectionWhenTheEditReachesTheWorkingCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorTypingSelectionTest::RunTest(const FString& Parameters)
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
	Rules->SelectAllText();
	PauseTyping(1.0f);
	TestEqualSensitive(TEXT("The working copy holds the typed text"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestTrue(TEXT("The Rules selection survives the edit reaching the working copy"), Rules->AnyTextSelected());
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorDelegatesTest,
	"Cambriana.Editor.GrammarEditor.WidgetCallbacksDoNotRunOnceTheEditorIsGone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorDelegatesTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TWeakPtr<IToolkit> Editor = Toolkit->AsShared();
	const FOnTextCommitted RulesCommitted = Toolkit->MakeRulesCommittedDelegate();
	const FIsPropertyVisible DetailsPropertyVisible = Toolkit->MakeDetailsPropertyVisibleDelegate();
	TestTrue(TEXT("The Rules commit callback is bound while the editor exists"), RulesCommitted.IsBound());
	TestTrue(TEXT("The Details visibility callback is bound while the editor exists"), DetailsPropertyVisible.IsBound());

	Toolkit = nullptr;
	CloseGrammarEditor(Grammar);
	if (!TestFalse(TEXT("Closing the grammar editor destroys it"), Editor.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("The Rules commit callback is unbound once the editor is gone"), RulesCommitted.IsBound());
	TestFalse(TEXT("The Details visibility callback is unbound once the editor is gone"), DetailsPropertyVisible.IsBound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewDefaultTest,
	"Cambriana.Editor.GrammarEditor.Preview.ViewportShowsTheTransientResultOfTheDefaultGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewDefaultTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarInPackage(TEXT("/Game/CambrianaEditorTests/PreviewedGrammar"));
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammar(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = Toolkit->GetPreview();
	const UDynamicMeshComponent* Mesh = Preview.GetMeshComponent();
	TestTrue(TEXT("The grammar has a package of its own"), Grammar->GetPackage() != GetTransientPackage());
	if (TestNotNull(TEXT("The preview has a mesh"), Mesh))
	{
		TestTrue(TEXT("The preview mesh is in the viewport's scene"), Mesh->IsRegistered() && Mesh->GetWorld() == Preview.GetWorld());
		TestEqual(TEXT("The default grammar's 4 segments in one chain at the asset's 8 sides"), Mesh->GetMesh()->VertexCount(), TubeVertices(4, 8));
		TestFalse(TEXT("Nothing of the preview is in the grammar's package"), Mesh->IsIn(Grammar->GetPackage()));
	}
	TestEqual(TEXT("The preview built once"), Preview.GetBuildCount(), 1);
	TestEqualSensitive(TEXT("The preview builds the asset's text"), Preview.GetLastBuildInput().GrammarText, Grammar->GrammarText);
	TestEqual(TEXT("The preview builds with the asset's seed"), Preview.GetLastBuildInput().Seed, Grammar->Seed);
	DiscardGrammar(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewRulesEditTest,
	"Cambriana.Editor.GrammarEditor.Preview.RulesEditRebuildsThePreviewOnceAndUndoRestoresIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewRulesEditTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()) || !TestNotNull(TEXT("The preview has a mesh"), Toolkit->GetPreview().GetMeshComponent()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = Toolkit->GetPreview();
	const int32 BuildsBefore = Preview.GetBuildCount();

	TypeOverRules(*Rules);
	TestEqual(TEXT("Typing alone does not rebuild the preview"), Preview.GetBuildCount(), BuildsBefore);
	PauseTyping(1.0f);
	TestEqual(TEXT("The Rules edit rebuilt the preview once"), Preview.GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("The edited grammar's 2 segments in one chain at 8 sides"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), TubeVertices(2, 8));

	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo rebuilt the preview once"), Preview.GetBuildCount(), BuildsBefore + 2);
	TestEqual(TEXT("Undo restores the default grammar's preview"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), TubeVertices(4, 8));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewDetailsEditTest,
	"Cambriana.Editor.GrammarEditor.Preview.DetailsSettingChangeToTheWorkingCopyRebuildsThePreviewAndUndoRestoresIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewDetailsEditTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TypedGrammarText;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit) || !TestNotNull(TEXT("The preview has a mesh"), Toolkit->GetPreview().GetMeshComponent()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = Toolkit->GetPreview();
	TestEqual(TEXT("2 segments at the asset's 8 sides"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), TubeVertices(2, 8));
	const int32 BuildsBefore = Preview.GetBuildCount();

	EditGrammarSides(Toolkit->GetWorkingCopy(), 3);
	TestEqual(TEXT("The Details change rebuilt the preview once"), Preview.GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("2 segments at the working copy's new 3 sides"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), TubeVertices(2, 3));
	TestEqual(TEXT("The grammar keeps its 8 sides"), Grammar->Sides, 8);

	TestTrue(TEXT("The change can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo rebuilt the preview once"), Preview.GetBuildCount(), BuildsBefore + 2);
	TestEqual(TEXT("Undo restores 8 sides"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), TubeVertices(2, 8));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewErrorsTest,
	"Cambriana.Editor.GrammarEditor.Preview.TextWithErrorsShowsAnEmptyPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewErrorsTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit) || !TestNotNull(TEXT("The preview has a mesh"), Toolkit->GetPreview().GetMeshComponent()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = Toolkit->GetPreview();
	TestTrue(TEXT("The default grammar's preview has geometry"), Preview.GetMeshComponent()->GetMesh()->TriangleCount() > 0);

	AddExpectedMessagePlain(TEXT("Grammar error at line"), ELogVerbosity::Error);
	Toolkit->CommitRulesText(FText::FromString(TEXT("20\n25\n50\n")));
	TestTrue(TEXT("The build reports errors"), Preview.GetLastBuildReport().bHasErrors);
	TestEqual(TEXT("The preview has no vertices"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), 0);
	TestEqual(TEXT("The preview has no triangles"), Preview.GetMeshComponent()->GetMesh()->TriangleCount(), 0);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewOtherGrammarTest,
	"Cambriana.Editor.GrammarEditor.Preview.ChangesThatLeaveTheBuildInputAloneDoNotRebuildThePreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewOtherGrammarTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	UCambrianaGrammar* Other = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = Toolkit->GetPreview();
	const int32 BuildsBefore = Preview.GetBuildCount();

	EditGrammarSides(Other, 3);
	TestEqual(TEXT("Another grammar's change does not rebuild the preview"), Preview.GetBuildCount(), BuildsBefore);
	EditGrammarSides(Toolkit->GetWorkingCopy(), Grammar->Sides);
	TestEqual(TEXT("A change to the same value does not rebuild the preview"), Preview.GetBuildCount(), BuildsBefore);
	CloseGrammarEditor(Grammar);
	return true;
}

namespace
{
	// Positions are hand-counted from docs/grammar-spec.md (Diagnostics: lines and byte columns from 1, the position each
	// code is reported at) and, for legacy text, from the end of the text; descriptions are DescribeCode's text per code.
	const TCHAR* const SyntaxErrorText = TEXT("axiom X\ngrow {\nX > F\n}\n");
	const TCHAR* const SyntaxErrorMessage = TEXT("line 3, column 3: syntax error");
	const TCHAR* const UnreachableRuleText = TEXT("axiom A\ngrow {\n    A -> F A\n    A -> F\n}\n");
	const TCHAR* const UnreachableRuleMessage = TEXT("line 4, column 5: unreachable rule");

	int32 CountMessages(const IMessageLogListing& Listing, EMessageSeverity::Type Severity, const FString& Text)
	{
		int32 Count = 0;
		for (const TSharedRef<FTokenizedMessage>& Message : Listing.GetFilteredMessages())
		{
			Count += Message->GetSeverity() == Severity && Message->ToText().ToString() == Text;
		}
		return Count;
	}

	bool DiagnosticsAreInTheRulesTab(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		return RulesTabWidgetOfType(Toolkit, TEXT("SMessageLogListing")).IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarDiagnosticsErrorTest,
	"Cambriana.Editor.GrammarEditor.Diagnostics.RulesEditWithAnErrorShowsTheErrorAtItsLineAndColumnInTheRulesTab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarDiagnosticsErrorTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestTrue(TEXT("The Rules tab shows the grammar's diagnostics"), DiagnosticsAreInTheRulesTab(*Toolkit));
	const IMessageLogListing& Diagnostics = Toolkit->GetRulesDiagnostics();
	TestEqual(TEXT("The default grammar has no diagnostics"), Diagnostics.GetFilteredMessages().Num(), 0);

	AddExpectedMessagePlain(TEXT("Grammar error at line 3, column 3: syntax error"), ELogVerbosity::Error);
	Rules->SelectAllText();
	Rules->InsertTextAtCursor(FString(SyntaxErrorText));
	PauseTyping(1.0f);
	TestEqual(TEXT("The syntax error shows as an error at line 3, column 3"),
		CountMessages(Diagnostics, EMessageSeverity::Error, SyntaxErrorMessage), 1);
	TestEqual(TEXT("It does not show as a warning"), CountMessages(Diagnostics, EMessageSeverity::Warning, SyntaxErrorMessage), 0);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarDiagnosticsWarningTest,
	"Cambriana.Editor.GrammarEditor.Diagnostics.RulesEditWithAWarningShowsTheWarningAtItsLineAndColumn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarDiagnosticsWarningTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const IMessageLogListing& Diagnostics = Toolkit->GetRulesDiagnostics();

	AddExpectedMessagePlain(TEXT("Grammar warning at line 4, column 5: unreachable rule"), ELogVerbosity::Warning);
	Toolkit->CommitRulesText(FText::FromString(UnreachableRuleText));
	TestEqual(TEXT("One diagnostic shows"), Diagnostics.GetFilteredMessages().Num(), 1);
	TestEqual(TEXT("The unreachable rule shows as a warning at line 4, column 5"),
		CountMessages(Diagnostics, EMessageSeverity::Warning, UnreachableRuleMessage), 1);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarDiagnosticsFixedTest,
	"Cambriana.Editor.GrammarEditor.Diagnostics.ErrorsShownWhenTheEditorOpensGoAwayWhenTheRulesAreFixedAndComeBackOnUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarDiagnosticsFixedTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = SyntaxErrorText;
	AddExpectedMessagePlain(TEXT("Grammar error at line 3, column 3: syntax error"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const IMessageLogListing& Diagnostics = Toolkit->GetRulesDiagnostics();
	TestEqual(TEXT("The error shows when the editor opens"), CountMessages(Diagnostics, EMessageSeverity::Error, SyntaxErrorMessage), 1);

	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	TestEqual(TEXT("Fixing the rules clears every diagnostic"), Diagnostics.GetFilteredMessages().Num(), 0);

	TestTrue(TEXT("The fix can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo shows the error again"), CountMessages(Diagnostics, EMessageSeverity::Error, SyntaxErrorMessage), 1);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarDiagnosticsSeveralTest,
	"Cambriana.Editor.GrammarEditor.Diagnostics.SeveralDiagnosticsAllShowInReportOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarDiagnosticsSeveralTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const IMessageLogListing& Diagnostics = Toolkit->GetRulesDiagnostics();

	AddExpectedMessagePlain(TEXT("Grammar warning at line 4, column 5: unreachable rule"), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("Grammar warning at line 5, column 5: unreachable rule"), ELogVerbosity::Warning);
	Toolkit->CommitRulesText(FText::FromString(TEXT("axiom A\ngrow {\n    A -> F A\n    A -> F\n    A -> F F\n}\n")));
	TestEqual(TEXT("The rule on line 4 shows"), CountMessages(Diagnostics, EMessageSeverity::Warning, UnreachableRuleMessage), 1);
	TestEqual(TEXT("The rule on line 5 shows"),
		CountMessages(Diagnostics, EMessageSeverity::Warning, TEXT("line 5, column 5: unreachable rule")), 1);

	const TArray<FCambrianaDiagnostic>& Reported = Toolkit->GetPreview().GetLastBuildReport().Diagnostics;
	const TArray<TSharedRef<FTokenizedMessage>>& Shown = Diagnostics.GetFilteredMessages();
	if (TestEqual(TEXT("Every reported diagnostic shows"), Shown.Num(), Reported.Num()))
	{
		for (int32 Index = 0; Index < Shown.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("Diagnostic %d shows in report order"), Index),
				Shown[Index]->ToText().ToString().StartsWith(FString::Printf(TEXT("line %llu, column %llu:"), Reported[Index].Line, Reported[Index].Column)));
		}
	}
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarDiagnosticsLegacyTest,
	"Cambriana.Editor.GrammarEditor.Diagnostics.LegacyTextShowsItsErrorAndWarningsAtTheirLineAndColumnOrProductionPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarDiagnosticsLegacyTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const IMessageLogListing& Diagnostics = Toolkit->GetRulesDiagnostics();

	// Three setting lines and no axiom: the text ends at line 4, column 1.
	AddExpectedMessagePlain(TEXT("Grammar error at line 4, column 1: missing axiom"), ELogVerbosity::Error);
	Toolkit->CommitRulesText(FText::FromString(TEXT("20\n25\n50\n")));
	TestEqual(TEXT("One diagnostic shows"), Diagnostics.GetFilteredMessages().Num(), 1);
	TestEqual(TEXT("The missing axiom shows as an error at the end of the text"),
		CountMessages(Diagnostics, EMessageSeverity::Error, TEXT("line 4, column 1: missing axiom")), 1);

	// The basic angle line is not a number (FMT-legacy-ls.md, File structure 2).
	AddExpectedMessagePlain(TEXT("Grammar warning at line 2, column 1: invalid number"), ELogVerbosity::Warning);
	Toolkit->CommitRulesText(FText::FromString(TEXT("4\nabc\n50\nA\nA=F^'A\n@\n")));
	TestEqual(TEXT("The invalid number shows as a warning at line 2, column 1"),
		CountMessages(Diagnostics, EMessageSeverity::Warning, TEXT("line 2, column 1: invalid number")), 1);

	// F at production position 0, the unmatched ] at 1 (grammar-spec.md, Diagnostics: codes 6, 7 and 8).
	AddExpectedMessagePlain(TEXT("Grammar warning at production position 1: unmatched ']'"), ELogVerbosity::Warning);
	Toolkit->CommitRulesText(FText::FromString(TEXT("1\n25\n50\nF]\n@\n")));
	TestEqual(TEXT("One diagnostic shows"), Diagnostics.GetFilteredMessages().Num(), 1);
	TestEqual(TEXT("The unmatched pop shows as a warning at its production position, having no line"),
		CountMessages(Diagnostics, EMessageSeverity::Warning, TEXT("production position 1: unmatched ']'")), 1);
	CloseGrammarEditor(Grammar);
	return true;
}

namespace
{
	// Turning around before drawing sends the chain below the turtle's start point.
	const TCHAR* const DownwardGrammarText = TEXT("axiom | F F\n");

	TSharedPtr<SEditorViewport> ViewportOf(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		const TSharedPtr<SDockTab> Tab = Toolkit.GetTabManager()->FindExistingLiveTab(FTabId(FCambrianaGrammarEditorToolkit::ViewportTabId));
		return Tab ? StaticCastSharedRef<SEditorViewport>(Tab->GetContent()) : TSharedPtr<SEditorViewport>();
	}

	TSharedPtr<FEditorViewportClient> ViewportClientOf(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		const TSharedPtr<SEditorViewport> Viewport = ViewportOf(Toolkit);
		return Viewport ? Viewport->GetViewportClient() : nullptr;
	}

	FBox PreviewVertexBounds(const FCambrianaGrammarPreview& Preview)
	{
		FBox Bounds(ForceInit);
		const UE::Geometry::FDynamicMesh3* Mesh = Preview.GetMeshComponent()->GetMesh();
		for (const int32 VertexId : Mesh->VertexIndicesItr())
		{
			Bounds += Mesh->GetVertex(VertexId);
		}
		return Bounds;
	}

	double FloorZ(const FCambrianaGrammarPreview& Preview)
	{
		return Preview.GetScene().GetFloorMeshComponent()->GetComponentLocation().Z;
	}

	// Regression pin of engine behaviour, not a spec value: the expected camera re-derives FEditorViewportClient::FocusViewportOnBox,
	// which keeps the rotation and backs away from the box centre until the box's half-diagonal, widened by the aspect ratio when the
	// view is wider than tall, fills half the horizontal field of view. The spec says only that the camera frames the mesh's bounds
	// with that function; the engine's fit lets near corners of the bounds reach slightly outside the frustum, so no exact
	// containment check holds.
	void TestFramesBox(FAutomationTestBase& Test, const FEditorViewportClient& Client, const FBox& Box)
	{
		const FVector Centre = Box.GetCenter();
		const FIntPoint Size = Client.Viewport->GetSizeXY();
		const double Aspect = Size.X > 0 && Size.Y > 0 ? Client.Viewport->GetDesiredAspectRatio() : Client.AspectRatio;
		const double Distance = Box.GetExtent().Size() * FMath::Max(Aspect, 1.0) / FMath::Tan(FMath::DegreesToRadians(Client.ViewFOV / 2.0));
		const FVector Expected = Centre - Client.GetViewRotation().Vector() * Distance;
		Test.TestTrue(TEXT("The camera looks at the centre of the mesh"), Client.GetLookAtLocation().Equals(Centre, 0.01));
		Test.TestTrue(FString::Printf(TEXT("The camera frames the mesh from %s, expected %s"), *Client.GetViewLocation().ToString(), *Expected.ToString()),
			Client.GetViewLocation().Equals(Expected, 0.01));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewRedrawTest,
	"Cambriana.Editor.GrammarEditor.Preview.RebuildRedrawsTheViewportWithoutInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewRedrawTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<FEditorViewportClient> Client = Toolkit ? ViewportClientOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows a viewport"), Client.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const int32 BuildsBefore = Toolkit->GetPreview().GetBuildCount();

	Client->bNeedsRedraw = false;
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	TestEqual(TEXT("The Rules edit rebuilt the preview"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);
	TestTrue(TEXT("The rebuild asks the viewport to redraw"), Client->bNeedsRedraw);

	Client->bNeedsRedraw = false;
	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestTrue(TEXT("The rebuild after undo asks the viewport to redraw"), Client->bNeedsRedraw);

	Client->bNeedsRedraw = false;
	EditGrammarSides(Toolkit->GetWorkingCopy(), Grammar->Sides);
	TestFalse(TEXT("A change that does not rebuild does not ask for a redraw"), Client->bNeedsRedraw);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarPreviewFloorTest,
	"Cambriana.Editor.GrammarEditor.Preview.MeshLowestPointRestsOnTheFloorAfterEveryRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarPreviewFloorTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = DownwardGrammarText;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit) || !TestNotNull(TEXT("The preview has a mesh"), Toolkit->GetPreview().GetMeshComponent()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = Toolkit->GetPreview();
	const FBox Downward = PreviewVertexBounds(Preview);
	TestTrue(TEXT("The downward grammar's mesh reaches below the origin"), Downward.IsValid && Downward.Min.Z < -1.0);
	TestEqual(TEXT("The floor is at the downward mesh's lowest point"), FloorZ(Preview), Downward.Min.Z, 0.001);
	TestTrue(TEXT("The preview mesh stays at the origin"), Preview.GetMeshComponent()->GetComponentLocation().IsZero());

	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	const FBox Upward = PreviewVertexBounds(Preview);
	TestTrue(TEXT("The upward grammar's lowest point is above the downward one's"), Upward.IsValid && Upward.Min.Z > Downward.Min.Z + 1.0);
	TestEqual(TEXT("The floor follows the rebuilt mesh's lowest point"), FloorZ(Preview), Upward.Min.Z, 0.001);

	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("After undo the floor is back at the downward mesh's lowest point"), FloorZ(Preview), Downward.Min.Z, 0.001);

	AddExpectedMessagePlain(TEXT("Grammar error at line"), ELogVerbosity::Error);
	Toolkit->CommitRulesText(FText::FromString(TEXT("20\n25\n50\n")));
	TestEqual(TEXT("An empty preview puts the floor at the origin"), FloorZ(Preview), 0.0, 0.001);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarViewportOpenFramesTest,
	"Cambriana.Editor.GrammarEditor.Viewport.OpeningTheEditorFramesTheMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarViewportOpenFramesTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = DownwardGrammarText;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<FEditorViewportClient> Client = Toolkit ? ViewportClientOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows a viewport"), Client.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestFramesBox(*this, *Client, PreviewVertexBounds(Toolkit->GetPreview()));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarViewportFocusTest,
	"Cambriana.Editor.GrammarEditor.Viewport.PressingFFramesTheMeshAgain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarViewportFocusTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = DownwardGrammarText;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SEditorViewport> Viewport = Toolkit ? ViewportOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows a viewport"), Viewport.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TSharedPtr<FEditorViewportClient> Client = Viewport->GetViewportClient();
	Client->SetViewLocation(FVector(5000.0, -3000.0, 2000.0));
	Client->SetLookAtLocation(FVector(4000.0, -3000.0, 2000.0));

	const FKeyEvent PressF(EKeys::F, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("F runs a viewport command"), Viewport->GetCommandList()->ProcessCommandBindings(PressF));

	const TWeakPtr<FEditorViewportClient> WeakClient = Client;
	const TWeakPtr<FCambrianaGrammarEditorToolkit> WeakToolkit = StaticCastSharedRef<FCambrianaGrammarEditorToolkit>(Toolkit->AsShared());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Grammar, WeakClient, WeakToolkit]()
	{
		const TSharedPtr<FEditorViewportClient> FramedClient = WeakClient.Pin();
		const TSharedPtr<FCambrianaGrammarEditorToolkit> FramedToolkit = WeakToolkit.Pin();
		if (FramedClient && FramedToolkit)
		{
			TestFramesBox(*this, *FramedClient, PreviewVertexBounds(FramedToolkit->GetPreview()));
		}
		else
		{
			AddError(TEXT("The grammar editor or its viewport closed before the camera finished moving"));
		}
		CloseGrammarEditor(Grammar);
	}, 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarViewportEditKeepsCameraTest,
	"Cambriana.Editor.GrammarEditor.Viewport.EditsDoNotMoveTheCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarViewportEditKeepsCameraTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<FEditorViewportClient> Client = Toolkit ? ViewportClientOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows a viewport"), Client.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const FVector Location = Client->GetViewLocation();
	const FVector LookAt = Client->GetLookAtLocation();
	const FRotator Rotation = Client->GetViewRotation();
	const int32 BuildsBefore = Toolkit->GetPreview().GetBuildCount();

	Toolkit->CommitRulesText(FText::FromString(DownwardGrammarText));
	TestEqual(TEXT("The Rules edit rebuilt the preview"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("The camera stays where it was"), Client->GetViewLocation(), Location);
	TestEqual(TEXT("The camera still looks at the same point"), Client->GetLookAtLocation(), LookAt);
	TestEqual(TEXT("The camera keeps its rotation"), Client->GetViewRotation(), Rotation);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorHidesImportDataTest,
	"Cambriana.Editor.GrammarEditor.DetailsHidesTheImportDataASaveLeavesWithTheAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorHidesImportDataTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	const TSharedRef<IPropertyRowGenerator> AssetRows = MakeRows({ Grammar });
	// Details shows the import data's source file (UAssetImportData::SourceData) in its own File Path category.
	TestTrue(TEXT("The asset's own Details shows its import data"), AssetRows->GetRootTreeNodes().ContainsByPredicate([](const TSharedRef<IDetailTreeNode>& Node)
	{
		return Node->GetNodeName() == TEXT("File Path");
	}));

	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestFalse(TEXT("The grammar editor's Details hides the working copy's import data"),
		Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, AssetImportData)));
	TestFalse(TEXT("The grammar editor's Details hides the import data's source file"),
		Toolkit->IsDetailsPropertyVisible(TEXT("SourceData"), { GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, AssetImportData) }));
	TestTrue(TEXT("The grammar editor's Details still shows the size"), Toolkit->IsDetailsPropertyVisible(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, SizeValue)));
	CloseGrammarEditor(Grammar);
	return true;
}

namespace
{
	const TCHAR* const FirstRulesEdit = TEXT("generations 1\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	bool RunToolkitCommand(FCambrianaGrammarEditorToolkit& Toolkit, const TSharedPtr<FUICommandInfo>& Command)
	{
		return Toolkit.GetToolkitCommands()->TryExecuteAction(Command.ToSharedRef());
	}

	// Slate text boxes join lines with "\r\n" on Windows.
	FString RulesBoxText(const FCambrianaGrammarEditorToolkit& Toolkit)
	{
		return Toolkit.GetRulesText().ToString().Replace(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorUndoPendingRulesTest,
	"Cambriana.Editor.GrammarEditor.UndoDuringTheTypingPauseUndoesThePendingRulesEditAsOneStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorUndoPendingRulesTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	Toolkit->CommitRulesText(FText::FromString(FirstRulesEdit));
	TypeOverRules(*Rules);

	RunToolkitCommand(*Toolkit, FGenericCommands::Get().Undo);
	TestEqualSensitive(TEXT("Undo removes the typed edit from the working copy"), Toolkit->GetWorkingCopy()->GrammarText, FString(FirstRulesEdit));
	TestEqualSensitive(TEXT("Rules shows the text from before the typing"), RulesBoxText(*Toolkit), FString(FirstRulesEdit));

	RunToolkitCommand(*Toolkit, FGenericCommands::Get().Redo);
	TestEqualSensitive(TEXT("Redo brings the typed edit back"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));

	RunToolkitCommand(*Toolkit, FGenericCommands::Get().Undo);
	RunToolkitCommand(*Toolkit, FGenericCommands::Get().Undo);
	TestEqualSensitive(TEXT("A second undo removes the earlier Rules edit"), Toolkit->GetWorkingCopy()->GrammarText, FString(UCambrianaGrammarFactory::DefaultGrammarText));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorOutsideUndoKeepsTypingTest,
	"Cambriana.Editor.GrammarEditor.AnUndoFromOutsideTheEditorDuringTheTypingPauseKeepsTheTypedText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarEditorOutsideUndoKeepsTypingTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	Toolkit->CommitRulesText(FText::FromString(FirstRulesEdit));
	TypeOverRules(*Rules);

	TestTrue(TEXT("The earlier Rules edit can be undone"), GEditor->UndoTransaction());
	TestEqualSensitive(TEXT("Rules keeps the typed text"), RulesBoxText(*Toolkit), FString(TypedGrammarText));
	TestTrue(TEXT("The typed text is still unsaved"), Toolkit->HasUnsavedEdits());
	PauseTyping(FCambrianaGrammarEditorToolkit::RulesApplyDelaySeconds + 0.1f);
	TestEqualSensitive(TEXT("After the pause the typed text reaches the working copy"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	CloseGrammarEditor(Grammar);
	return true;
}

#endif
