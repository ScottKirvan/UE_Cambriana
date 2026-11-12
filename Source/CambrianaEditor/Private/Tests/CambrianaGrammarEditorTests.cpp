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
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "SEditorViewport.h"
#include "ScopedTransaction.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarEditorTypingSelectionTest,
	"Cambriana.Editor.GrammarEditor.RulesTypingKeepsTheRulesSelectionWhenTheEditReachesTheGrammar",
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
	TestEqual(TEXT("The grammar holds the typed text"), Grammar->GrammarText, FString(TypedGrammarText));
	TestTrue(TEXT("The Rules selection survives the edit reaching the grammar"), Rules->AnyTextSelected());
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

namespace
{
	// Each tube ring has one vertex per side, and an unbranched chain of N segments has N + 1 rings (SDD-surfacing.md, Tubes).
	constexpr int32 TubeVertices(int32 Segments, int32 Sides)
	{
		return (Segments + 1) * Sides;
	}

	void EditGrammarSides(UCambrianaGrammar* Grammar, int32 Sides)
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Edit Sides")));
		FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Sides));
		Grammar->PreEditChange(Property);
		Grammar->Sides = Sides;
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Grammar->PostEditChangeProperty(Event);
	}
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
		CloseGrammarEditor(Grammar);
		UnloadPackage(Grammar->GetPackage());
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
	TestEqual(TEXT("The preview builds the asset's text"), Preview.GetLastBuildInput().GrammarText, Grammar->GrammarText);
	TestEqual(TEXT("The preview builds with the asset's seed"), Preview.GetLastBuildInput().Seed, Grammar->Seed);
	CloseGrammarEditor(Grammar);
	UnloadPackage(Grammar->GetPackage());
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
	"Cambriana.Editor.GrammarEditor.Preview.DetailsSettingChangeRebuildsThePreviewAndUndoRestoresIt",
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

	EditGrammarSides(Grammar, 3);
	TestEqual(TEXT("The Details change rebuilt the preview once"), Preview.GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("2 segments at the asset's new 3 sides"), Preview.GetMeshComponent()->GetMesh()->VertexCount(), TubeVertices(2, 3));

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
	EditGrammarSides(Grammar, Grammar->Sides);
	TestEqual(TEXT("A change to the same value does not rebuild the preview"), Preview.GetBuildCount(), BuildsBefore);
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

	// FEditorViewportClient::FocusViewportOnBox: the camera keeps its rotation and backs away from the box centre until the
	// box's half-diagonal, widened by the aspect ratio when the view is wider than tall, fills half the horizontal field of view.
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
	EditGrammarSides(Grammar, Grammar->Sides);
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
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Grammar, WeakClient, Toolkit]()
	{
		if (const TSharedPtr<FEditorViewportClient> FramedClient = WeakClient.Pin())
		{
			TestFramesBox(*this, *FramedClient, PreviewVertexBounds(Toolkit->GetPreview()));
		}
		else
		{
			AddError(TEXT("The viewport closed before the camera finished moving"));
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

#endif
