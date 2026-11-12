#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaLiveActor.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/AssetEditorSubsystem.h"

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

#endif
