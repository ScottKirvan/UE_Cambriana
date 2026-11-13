#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarCustomization.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarPreview.h"
#include "CambrianaLiveActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CloseAsking;
using Cambriana::Editor::Tests::CloseFromTab;
using Cambriana::Editor::Tests::CloseGrammarEditor;
using Cambriana::Editor::Tests::CreateGrammarInPackage;
using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::DestroyActor;
using Cambriana::Editor::Tests::EditGrammarSides;
using Cambriana::Editor::Tests::FindGrammarEditor;
using Cambriana::Editor::Tests::FindStoredValue;
using Cambriana::Editor::Tests::OpenGrammarEditor;
using Cambriana::Editor::Tests::PackageFilename;
using Cambriana::Editor::Tests::PlaceInCurrentLevel;
using Cambriana::Editor::Tests::PreviewVertexCount;
using Cambriana::Editor::Tests::RulesTextBoxOf;
using Cambriana::Editor::Tests::RunWithoutSavePrompt;
using Cambriana::Editor::Tests::TubeVertices;
using Cambriana::Editor::Tests::TypedGrammarText;
using Cambriana::Editor::Tests::TypeOverRules;
using Cambriana::Editor::Tests::UnloadPackage;
using Cambriana::Editor::Tests::AnswerSaveAs;
using Cambriana::Editor::Tests::AnswerUnsavedEdits;
using Cambriana::Editor::Tests::DiscardGrammarAndActor;

namespace
{
	// One chain of `count` segments (docs/grammar-spec.md, repeat).
	const TCHAR* const CountGrammarText = TEXT("structure param count = 3\n\naxiom repeat count { F }\n");

	FString DefaultText()
	{
		return UCambrianaGrammarFactory::DefaultGrammarText;
	}

	/** Rules, Details parameter and Details setting edits, as the editor's panels make them. */
	void EditInEveryPanel(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		Toolkit.CommitRulesText(FText::FromString(CountGrammarText));
		Cambriana::Editor::SetParameterValue(*Toolkit.GetWorkingCopy(), TEXT("count"), 5.0);
		EditGrammarSides(Toolkit.GetWorkingCopy(), 3);
	}

	void DeleteSavedPackage(const FString& Filename)
	{
		IFileManager::Get().Delete(*Filename);
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Filename));
	}

	const UCambrianaGrammar* LoadSaved(const TCHAR* PackageName, UPackage*& OutPackage)
	{
		OutPackage = IFileManager::Get().FileExists(*PackageFilename(PackageName)) ? LoadPackage(nullptr, PackageName, LOAD_None) : nullptr;
		return OutPackage ? FindObject<UCambrianaGrammar>(OutPackage, *FPackageName::GetShortName(PackageName)) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyPanelsTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.RulesDetailsAndViewportShowATransientWorkingCopyOfTheGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyPanelsTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = CountGrammarText;
	Grammar->Seed = 1111;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const UCambrianaGrammar* WorkingCopy = Toolkit->GetWorkingCopy();
	if (!TestNotNull(TEXT("The editor has a working copy"), WorkingCopy))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestTrue(TEXT("The working copy is not the grammar"), WorkingCopy != Grammar);
	TestTrue(TEXT("The working copy is in the transient package"), WorkingCopy->GetPackage() == GetTransientPackage());
	TestEqualSensitive(TEXT("The working copy holds the grammar's text"), WorkingCopy->GrammarText, FString(CountGrammarText));
	TestEqual(TEXT("The working copy holds the grammar's seed"), WorkingCopy->Seed, 1111u);
	TestTrue(TEXT("The editor still edits the grammar"), Toolkit->GetEditingObject() == Grammar);
	const TArray<TWeakObjectPtr<UObject>> DetailsObjects = Toolkit->GetDetailsObjects();
	TestTrue(TEXT("Details shows the working copy"), DetailsObjects.Num() == 1 && DetailsObjects[0].Get() == WorkingCopy);
	TestEqualSensitive(TEXT("The preview builds the working copy's text"), Toolkit->GetPreview().GetLastBuildInput().GrammarText, FString(CountGrammarText));
	TestFalse(TEXT("A freshly opened editor has no unsaved edits"), Toolkit->HasUnsavedEdits());
	TestTrue(TEXT("The tab shows no unsaved mark"), Toolkit->GetTabSuffix().IsEmpty());
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyEditsStayInTheEditorTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.EditsInEveryPanelReachThePreviewButNotTheGrammarItsPackageOrItsActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyEditsStayInTheEditorTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarInPackage(TEXT("/Game/CambrianaEditorTests/WorkingCopyGrammar"));
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammarAndActor(Grammar, Actor);
		return false;
	}
	Grammar->GetPackage()->SetDirtyFlag(false);
	const int32 ActorBuildsBefore = Actor->GetBuildCount();

	EditInEveryPanel(*Toolkit);
	TestEqual(TEXT("The preview shows the stored count 5 at 3 sides"), PreviewVertexCount(*Toolkit), TubeVertices(5, 3));
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, DefaultText());
	TestEqual(TEXT("The grammar stores no parameter value"), Grammar->ParameterValues.Num(), 0);
	TestEqual(TEXT("The grammar keeps its 8 sides"), Grammar->Sides, 8);
	TestFalse(TEXT("The grammar's package stays clean"), Grammar->GetPackage()->IsDirty());
	TestEqual(TEXT("The actor did not rebuild"), Actor->GetBuildCount(), ActorBuildsBefore);
	TestTrue(TEXT("The editor has unsaved edits"), Toolkit->HasUnsavedEdits());
	TestEqual(TEXT("The tab marks the unsaved edits"), Toolkit->GetTabSuffix().ToString(), FString(TEXT("*")));

	DiscardGrammarAndActor(Grammar, Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.SaveCopiesTheWorkingCopyIntoTheGrammarSavesItAndRebuildsItsActorsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaved");
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);

	UCambrianaGrammar* Grammar = CreateGrammarInPackage(PackageName);
	UPackage* Package = Grammar->GetPackage();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammarAndActor(Grammar, Actor);
		return false;
	}
	EditInEveryPanel(*Toolkit);
	const int32 ActorBuildsBefore = Actor->GetBuildCount();

	RunWithoutSavePrompt([Toolkit] { Toolkit->SaveAsset_Execute(); });
	TestEqualSensitive(TEXT("The grammar holds the edited text"), Grammar->GrammarText, FString(CountGrammarText));
	const FCambrianaParameterValue* Stored = FindStoredValue(*Grammar, TEXT("count"));
	TestTrue(TEXT("The grammar stores count = 5"), Stored && Stored->Value == 5.0);
	TestEqual(TEXT("The grammar holds 3 sides"), Grammar->Sides, 3);
	TestEqual(TEXT("The actor rebuilt once"), Actor->GetBuildCount(), ActorBuildsBefore + 1);
	TestEqualSensitive(TEXT("The actor built the saved text"), Actor->GetLastBuildInput().GrammarText, FString(CountGrammarText));
	TestTrue(TEXT("The package is on disk"), IFileManager::Get().FileExists(*Filename));
	TestFalse(TEXT("The saved package is clean"), Package->IsDirty());
	TestFalse(TEXT("Nothing is left unsaved"), Toolkit->HasUnsavedEdits());
	TestTrue(TEXT("The tab shows no unsaved mark"), Toolkit->GetTabSuffix().IsEmpty());

	RunWithoutSavePrompt([Toolkit] { Toolkit->SaveAsset_Execute(); });
	TestEqual(TEXT("Saving again without edits does not rebuild the actor"), Actor->GetBuildCount(), ActorBuildsBefore + 1);

	CloseGrammarEditor(Grammar);
	DestroyActor(Actor);
	TestTrue(TEXT("The package unloads"), UnloadPackage(Package));
	Grammar = nullptr;
	Package = nullptr;

	UPackage* LoadedPackage = nullptr;
	const UCambrianaGrammar* Loaded = LoadSaved(PackageName, LoadedPackage);
	if (TestNotNull(TEXT("The saved grammar loads again"), Loaded))
	{
		TestEqualSensitive(TEXT("The loaded grammar holds the edited text"), Loaded->GrammarText, FString(CountGrammarText));
		const FCambrianaParameterValue* LoadedCount = FindStoredValue(*Loaded, TEXT("count"));
		TestTrue(TEXT("The loaded grammar stores count = 5"), LoadedCount && LoadedCount->Value == 5.0);
		TestEqual(TEXT("The loaded grammar holds 3 sides"), Loaded->Sides, 3);
	}
	if (LoadedPackage)
	{
		UnloadPackage(LoadedPackage);
	}
	DeleteSavedPackage(Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyUndoAfterSaveTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.UndoPastASaveChangesOnlyTheWorkingCopyAndShowsUnsavedEditsAgain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyUndoAfterSaveTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopyUndoAfterSave");
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);
	UCambrianaGrammar* Grammar = CreateGrammarInPackage(PackageName);
	UPackage* Package = Grammar->GetPackage();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		DiscardGrammarAndActor(Grammar, Actor);
		return false;
	}
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	RunWithoutSavePrompt([Toolkit] { Toolkit->SaveAsset_Execute(); });
	TestEqualSensitive(TEXT("The save gives the grammar the edited text"), Grammar->GrammarText, FString(TypedGrammarText));
	TestTrue(TEXT("The package is on disk"), IFileManager::Get().FileExists(*Filename));
	TestFalse(TEXT("The saved package is clean"), Package->IsDirty());
	TestFalse(TEXT("Nothing is left unsaved"), Toolkit->HasUnsavedEdits());
	const int32 ActorBuildsAfterSave = Actor->GetBuildCount();

	TestTrue(TEXT("The edit can be undone after the save"), GEditor->UndoTransaction());
	TestEqualSensitive(TEXT("Undo restores the working copy's text"), Toolkit->GetWorkingCopy()->GrammarText, DefaultText());
	TestEqualSensitive(TEXT("Undo leaves the saved grammar's text"), Grammar->GrammarText, FString(TypedGrammarText));
	TestFalse(TEXT("Undo leaves the saved package clean"), Package->IsDirty());
	TestEqual(TEXT("Undo does not rebuild the actor"), Actor->GetBuildCount(), ActorBuildsAfterSave);
	TestTrue(TEXT("The undone edit is unsaved"), Toolkit->HasUnsavedEdits());
	TestEqual(TEXT("The tab marks the unsaved edit"), Toolkit->GetTabSuffix().ToString(), FString(TEXT("*")));

	TestTrue(TEXT("The edit can be redone"), GEditor->RedoTransaction());
	TestEqualSensitive(TEXT("Redo restores the working copy's edited text"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestFalse(TEXT("Redoing back to the saved text leaves nothing unsaved"), Toolkit->HasUnsavedEdits());
	DiscardGrammarAndActor(Grammar, Actor);
	DeleteSavedPackage(Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyExternalSaveTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ASaveOfTheGrammarFromElsewhereSavesItAsItIsAndKeepsTheUnsavedEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyExternalSaveTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopyExternal");
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);

	UCambrianaGrammar* Grammar = CreateGrammarInPackage(PackageName);
	UPackage* Package = Grammar->GetPackage();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		UnloadPackage(Package);
		return false;
	}
	Package->SetDirtyFlag(false);
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	TestFalse(TEXT("The edit leaves the grammar's package clean"), Package->IsDirty());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	TestTrue(TEXT("The package saves to disk"), UPackage::SavePackage(Package, Grammar, *Filename, SaveArgs));
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, DefaultText());
	TestEqualSensitive(TEXT("The working copy keeps the edited text"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestEqualSensitive(TEXT("Rules keeps the edited text"), Toolkit->GetRulesText().ToString(), FString(TypedGrammarText));
	TestTrue(TEXT("The edit is still unsaved"), Toolkit->HasUnsavedEdits());

	CloseGrammarEditor(Grammar);
	TestTrue(TEXT("The package unloads"), UnloadPackage(Package));
	Grammar = nullptr;
	Package = nullptr;

	UPackage* LoadedPackage = nullptr;
	const UCambrianaGrammar* Loaded = LoadSaved(PackageName, LoadedPackage);
	if (TestNotNull(TEXT("The saved grammar loads again"), Loaded))
	{
		TestEqualSensitive(TEXT("The saved grammar holds its text from before the edit"), Loaded->GrammarText, DefaultText());
	}
	if (LoadedPackage)
	{
		UnloadPackage(LoadedPackage);
	}
	DeleteSavedPackage(Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCloseCleanTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ClosingWithoutUnsavedEditsDoesNotAsk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCloseCleanTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	int32 Asked = 0;
	AnswerUnsavedEdits(*Toolkit, EAppReturnType::Cancel, Asked);
	CloseAsking(Grammar);
	TestEqual(TEXT("Closing does not ask"), Asked, 0);
	if (!TestNull(TEXT("The editor closes"), FindGrammarEditor(Grammar)))
	{
		CloseGrammarEditor(Grammar);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCloseSaveTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ClosingWithUnsavedEditsAsksAndSavingSavesThemIntoTheGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCloseSaveTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopyClosedSaved");
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);

	UCambrianaGrammar* Grammar = CreateGrammarInPackage(PackageName);
	UPackage* Package = Grammar->GetPackage();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	const TSharedPtr<SMultiLineEditableTextBox> Rules = Toolkit ? RulesTextBoxOf(*Toolkit) : nullptr;
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestTrue(TEXT("The grammar editor shows Rules"), Rules.IsValid()))
	{
		DiscardGrammarAndActor(Grammar, Actor);
		return false;
	}
	TypeOverRules(*Rules);
	const int32 ActorBuildsBefore = Actor->GetBuildCount();
	int32 Asked = 0;
	FString Message;
	Toolkit->SetUnsavedEditsPrompt([&Asked, &Message](EAppMsgType::Type, const FText& Text) { ++Asked; Message = Text.ToString(); return EAppReturnType::Yes; });

	RunWithoutSavePrompt([Grammar] { CloseAsking(Grammar); });
	TestEqual(TEXT("Closing asks once"), Asked, 1);
	TestTrue(FString::Printf(TEXT("The question names the grammar: %s"), *Message), Message.Contains(Grammar->GetPathName()));
	TestNull(TEXT("Saving closes the editor"), FindGrammarEditor(Grammar));
	TestEqualSensitive(TEXT("The grammar holds the typed text"), Grammar->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("The actor rebuilt once"), Actor->GetBuildCount(), ActorBuildsBefore + 1);
	TestTrue(TEXT("The package is on disk"), IFileManager::Get().FileExists(*Filename));
	TestFalse(TEXT("The saved package is clean"), Package->IsDirty());

	if (FindGrammarEditor(Grammar))
	{
		CloseGrammarEditor(Grammar);
	}
	DestroyActor(Actor);
	UnloadPackage(Package);
	DeleteSavedPackage(Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCloseDiscardTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ClosingWithUnsavedEditsAndDiscardingLeavesTheGrammarAndItsActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCloseDiscardTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("A live actor was placed"), Actor) || !TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		DestroyActor(Actor);
		return false;
	}
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	const int32 ActorBuildsBefore = Actor->GetBuildCount();
	int32 Asked = 0;
	AnswerUnsavedEdits(*Toolkit, EAppReturnType::No, Asked);

	CloseAsking(Grammar);
	TestEqual(TEXT("Closing asks once"), Asked, 1);
	TestNull(TEXT("Discarding closes the editor"), FindGrammarEditor(Grammar));
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, DefaultText());
	TestEqual(TEXT("The actor did not rebuild"), Actor->GetBuildCount(), ActorBuildsBefore);
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCloseCancelTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.CancellingTheCloseQuestionKeepsTheEditorAndItsUnsavedEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCloseCancelTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	int32 Asked = 0;
	EAppMsgType::Type Offered = EAppMsgType::Ok;
	Toolkit->SetUnsavedEditsPrompt([&Asked, &Offered](EAppMsgType::Type Buttons, const FText&) { ++Asked; Offered = Buttons; return EAppReturnType::Cancel; });

	TestFalse(TEXT("The tab does not close"), CloseFromTab(*Toolkit));
	TestEqual(TEXT("Closing asks once"), Asked, 1);
	TestTrue(TEXT("Closing the tab offers Cancel"), Offered == EAppMsgType::YesNoCancel);
	if (!TestTrue(TEXT("Cancelling keeps the editor open"), FindGrammarEditor(Grammar) == Toolkit))
	{
		return false;
	}
	TestEqualSensitive(TEXT("The working copy keeps the edited text"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, DefaultText());
	TestTrue(TEXT("The edit is still unsaved"), Toolkit->HasUnsavedEdits());
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyFollowsGrammarTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.AGrammarChangeFromElsewhereReachesAWorkingCopyWithoutUnsavedEditsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyFollowsGrammarTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	EditGrammarSides(Grammar, 3);
	TestEqual(TEXT("Without unsaved edits the working copy takes the grammar's 3 sides"), Toolkit->GetWorkingCopy()->Sides, 3);
	TestEqual(TEXT("The preview shows the default grammar's 4 segments at 3 sides"), PreviewVertexCount(*Toolkit), TubeVertices(4, 3));
	TestFalse(TEXT("Following the grammar leaves nothing unsaved"), Toolkit->HasUnsavedEdits());

	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	EditGrammarSides(Grammar, 5);
	TestEqual(TEXT("With unsaved edits the working copy keeps its 3 sides"), Toolkit->GetWorkingCopy()->Sides, 3);
	TestEqualSensitive(TEXT("With unsaved edits the working copy keeps its text"), Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("The preview still shows the edited 2 segments at 3 sides"), PreviewVertexCount(*Toolkit), TubeVertices(2, 3));
	TestTrue(TEXT("The edits are still unsaved"), Toolkit->HasUnsavedEdits());
	CloseGrammarEditor(Grammar);
	return true;
}

namespace
{
	/** A placed actor and an open editor holding edits in every panel, for a grammar in a package of its own. */
	struct FEditedGrammar
	{
		UCambrianaGrammar* Grammar = nullptr;
		ACambrianaLiveActor* Actor = nullptr;
		FCambrianaGrammarEditorToolkit* Toolkit = nullptr;
		int32 ActorBuildsAfterEdits = 0;
		bool bPackageDirty = false;

		explicit FEditedGrammar(const TCHAR* PackageName)
		{
			Grammar = CreateGrammarInPackage(PackageName);
			Actor = PlaceInCurrentLevel(Grammar);
			Toolkit = OpenGrammarEditor(Grammar);
			if (Toolkit)
			{
				Grammar->GetPackage()->SetDirtyFlag(false);
				EditInEveryPanel(*Toolkit);
			}
			ActorBuildsAfterEdits = Actor ? Actor->GetBuildCount() : 0;
		}

		~FEditedGrammar()
		{
			DestroyActor(Actor);
			if (Grammar)
			{
				CloseGrammarEditor(Grammar);
				UnloadPackage(Grammar->GetPackage());
			}
		}

		/** Marks the grammar's package dirty, as an unsaved change made elsewhere would. */
		void DirtyPackage()
		{
			Grammar->GetPackage()->SetDirtyFlag(true);
			bPackageDirty = true;
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("A live actor was placed"), Actor) && Test.TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit);
		}

		void TestGrammarAndActorUnchanged(FAutomationTestBase& Test) const
		{
			Test.TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, DefaultText());
			Test.TestEqual(TEXT("The grammar stores no parameter value"), Grammar->ParameterValues.Num(), 0);
			Test.TestEqual(TEXT("The grammar keeps its 8 sides"), Grammar->Sides, 8);
			Test.TestEqual(TEXT("The grammar's package keeps its dirty state"), Grammar->GetPackage()->IsDirty(), bPackageDirty);
			Test.TestEqual(TEXT("The actor did not rebuild"), Actor->GetBuildCount(), ActorBuildsAfterEdits);
		}

		void TestEditorKeepsTheEdits(FAutomationTestBase& Test) const
		{
			if (!Test.TestTrue(TEXT("The editor stays open"), FindGrammarEditor(Grammar) == Toolkit))
			{
				return;
			}
			Test.TestEqualSensitive(TEXT("The working copy keeps the edited text"), Toolkit->GetWorkingCopy()->GrammarText, FString(CountGrammarText));
			Test.TestEqual(TEXT("The working copy keeps its 3 sides"), Toolkit->GetWorkingCopy()->Sides, 3);
			Test.TestTrue(TEXT("The edits are still unsaved"), Toolkit->HasUnsavedEdits());
		}
	};

	/** With -unattended the engine's save prompt (FEditorFileUtils::PromptForCheckoutAndSave) cancels, so a save does not complete. */
	bool SaveCancelsInThisRun(FAutomationTestBase& Test)
	{
		if (!FApp::IsUnattended())
		{
			Test.AddError(TEXT("Needs -unattended, where the engine cancels the save"));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveCancelledTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ASaveThatDoesNotCompleteLeavesTheGrammarItsPackageAndItsActorsAndKeepsTheEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveCancelledTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaveCancelled");
	IFileManager::Get().Delete(*PackageFilename(PackageName));
	const FEditedGrammar Edited(PackageName);
	if (!Edited.IsValid(*this) || !SaveCancelsInThisRun(*this))
	{
		return false;
	}
	Edited.Toolkit->SaveAsset_Execute();
	Edited.TestGrammarAndActorUnchanged(*this);
	TestFalse(TEXT("Nothing is written"), IFileManager::Get().FileExists(*PackageFilename(PackageName)));
	Edited.TestEditorKeepsTheEdits(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCloseSaveCancelledTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ClosingAndSavingWhenTheSaveDoesNotCompleteKeepsTheEditorOpenAndLeavesTheGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCloseSaveCancelledTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopyCloseSaveCancelled");
	IFileManager::Get().Delete(*PackageFilename(PackageName));
	const FEditedGrammar Edited(PackageName);
	if (!Edited.IsValid(*this) || !SaveCancelsInThisRun(*this))
	{
		return false;
	}
	int32 Asked = 0;
	AnswerUnsavedEdits(*Edited.Toolkit, EAppReturnType::Yes, Asked);
	TestFalse(TEXT("The tab does not close"), CloseFromTab(*Edited.Toolkit));
	TestEqual(TEXT("Closing asks once"), Asked, 1);
	Edited.TestGrammarAndActorUnchanged(*this);
	Edited.TestEditorKeepsTheEdits(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveAsTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.SaveAsWritesTheEditsToTheNewAssetOnlyAndOpensIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveAsTest::RunTest(const FString& Parameters)
{
	const TCHAR* CopyPackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySavedAs");
	const FString CopyFilename = PackageFilename(CopyPackageName);
	IFileManager::Get().Delete(*CopyFilename);
	const FEditedGrammar Edited(TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsOriginal"));
	if (!Edited.IsValid(*this))
	{
		return false;
	}
	int32 Asked = 0;
	AnswerSaveAs(*Edited.Toolkit, CopyPackageName, Asked);

	RunWithoutSavePrompt([&Edited] { Edited.Toolkit->SaveAssetAs_Execute(); });
	TestEqual(TEXT("Save As asks for the new asset once"), Asked, 1);
	Edited.TestGrammarAndActorUnchanged(*this);
	TestNull(TEXT("The editor no longer edits the original"), FindGrammarEditor(Edited.Grammar));

	UPackage* CopyPackage = FindPackage(nullptr, CopyPackageName);
	UCambrianaGrammar* Copy = CopyPackage ? FindObject<UCambrianaGrammar>(CopyPackage, *FPackageName::GetShortName(CopyPackageName)) : nullptr;
	if (TestNotNull(TEXT("The new asset exists"), Copy))
	{
		TestEqualSensitive(TEXT("The new asset holds the edited text"), Copy->GrammarText, FString(CountGrammarText));
		const FCambrianaParameterValue* Stored = FindStoredValue(*Copy, TEXT("count"));
		TestTrue(TEXT("The new asset stores count = 5"), Stored && Stored->Value == 5.0);
		TestEqual(TEXT("The new asset holds 3 sides"), Copy->Sides, 3);
		TestTrue(TEXT("The new asset is a standalone public asset"), Copy->HasAllFlags(RF_Public | RF_Standalone) && !Copy->HasAnyFlags(RF_Transient));
		TestTrue(TEXT("The new asset is on disk"), IFileManager::Get().FileExists(*CopyFilename));
		TestFalse(TEXT("The new asset's package is clean"), CopyPackage->IsDirty());
		const FCambrianaGrammarEditorToolkit* CopyEditor = FindGrammarEditor(Copy);
		if (TestNotNull(TEXT("The editor opens on the new asset"), CopyEditor))
		{
			TestFalse(TEXT("The new asset's editor has nothing unsaved"), CopyEditor->HasUnsavedEdits());
		}
		CloseGrammarEditor(Copy);
	}
	if (CopyPackage)
	{
		UnloadPackage(CopyPackage);
	}
	DeleteSavedPackage(CopyFilename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveAsCancelledTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.CancellingSaveAsLeavesTheGrammarAndItsActorsAndKeepsTheEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveAsCancelledTest::RunTest(const FString& Parameters)
{
	const FEditedGrammar Edited(TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsCancelled"));
	if (!Edited.IsValid(*this))
	{
		return false;
	}
	int32 Asked = 0;
	AnswerSaveAs(*Edited.Toolkit, FString(), Asked);

	RunWithoutSavePrompt([&Edited] { Edited.Toolkit->SaveAssetAs_Execute(); });
	TestEqual(TEXT("Save As asks for the new asset once"), Asked, 1);
	Edited.TestGrammarAndActorUnchanged(*this);
	Edited.TestEditorKeepsTheEdits(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveAsLoadedAssetTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.SaveAsOntoALoadedAssetLeavesThatAssetAndItsActorsTellsTheUserAndKeepsTheEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveAsLoadedAssetTest::RunTest(const FString& Parameters)
{
	const TCHAR* ExistingPackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsExisting");
	UCambrianaGrammar* Existing = CreateGrammarInPackage(ExistingPackageName);
	Existing->GrammarText = TypedGrammarText;
	Existing->GetPackage()->SetDirtyFlag(false);
	ACambrianaLiveActor* ExistingActor = PlaceInCurrentLevel(Existing);
	const FEditedGrammar Edited(TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsOntoExisting"));
	if (!Edited.IsValid(*this) || !TestNotNull(TEXT("A live actor was placed for the existing asset"), ExistingActor))
	{
		DestroyActor(ExistingActor);
		UnloadPackage(Existing->GetPackage());
		return false;
	}
	const int32 ExistingBuilds = ExistingActor->GetBuildCount();
	Edited.Toolkit->SetSaveAsPrompt([ExistingPackageName](const UCambrianaGrammar&) { return FString(ExistingPackageName); });
	AddExpectedMessagePlain(TEXT("already exists"), ELogVerbosity::Warning);

	RunWithoutSavePrompt([&Edited] { Edited.Toolkit->SaveAssetAs_Execute(); });
	UPackage* ExistingPackage = FindPackage(nullptr, ExistingPackageName);
	TestTrue(TEXT("The existing asset is still the object in its package"),
		ExistingPackage && FindObject<UCambrianaGrammar>(ExistingPackage, *FPackageName::GetShortName(ExistingPackageName)) == Existing);
	TestEqualSensitive(TEXT("The existing asset keeps its text"), Existing->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("The existing asset keeps its 8 sides"), Existing->Sides, 8);
	TestFalse(TEXT("The existing asset's package stays clean"), Existing->GetPackage()->IsDirty());
	TestEqual(TEXT("The existing asset's actor did not rebuild"), ExistingActor->GetBuildCount(), ExistingBuilds);
	TestFalse(TEXT("Nothing is written for the existing asset"), IFileManager::Get().FileExists(*PackageFilename(ExistingPackageName)));
	Edited.TestGrammarAndActorUnchanged(*this);
	Edited.TestEditorKeepsTheEdits(*this);

	DestroyActor(ExistingActor);
	UnloadPackage(Existing->GetPackage());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveAsAssetOnDiskTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.SaveAsOntoAnAssetOnDiskLeavesItsFileTellsTheUserAndKeepsTheEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveAsAssetOnDiskTest::RunTest(const FString& Parameters)
{
	const TCHAR* ExistingPackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsOnDisk");
	const FString ExistingFilename = PackageFilename(ExistingPackageName);
	{
		UCambrianaGrammar* Existing = CreateGrammarInPackage(ExistingPackageName);
		Existing->GrammarText = TypedGrammarText;
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const bool bSaved = UPackage::SavePackage(Existing->GetPackage(), Existing, *ExistingFilename, SaveArgs);
		UnloadPackage(Existing->GetPackage());
		if (!TestTrue(TEXT("The existing asset is on disk"), bSaved))
		{
			DeleteSavedPackage(ExistingFilename);
			return false;
		}
	}
	TArray<uint8> BytesBefore;
	FFileHelper::LoadFileToArray(BytesBefore, *ExistingFilename);
	const FEditedGrammar Edited(TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsOntoDisk"));
	if (!Edited.IsValid(*this))
	{
		DeleteSavedPackage(ExistingFilename);
		return false;
	}
	Edited.Toolkit->SetSaveAsPrompt([ExistingPackageName](const UCambrianaGrammar&) { return FString(ExistingPackageName); });
	AddExpectedMessagePlain(TEXT("already exists"), ELogVerbosity::Warning);

	RunWithoutSavePrompt([&Edited] { Edited.Toolkit->SaveAssetAs_Execute(); });
	TArray<uint8> BytesAfter;
	FFileHelper::LoadFileToArray(BytesAfter, *ExistingFilename);
	TestTrue(TEXT("The existing asset's file is unchanged"), BytesAfter.Num() > 0 && BytesAfter == BytesBefore);
	TestNull(TEXT("No package is made at the existing asset's path"), FindPackage(nullptr, ExistingPackageName));
	Edited.TestGrammarAndActorUnchanged(*this);
	Edited.TestEditorKeepsTheEdits(*this);

	if (UPackage* Made = FindPackage(nullptr, ExistingPackageName))
	{
		UnloadPackage(Made);
	}
	DeleteSavedPackage(ExistingFilename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveCancelledDirtyTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.ASaveThatDoesNotCompleteLeavesAnAlreadyDirtyPackageDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveCancelledDirtyTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaveCancelledDirty");
	IFileManager::Get().Delete(*PackageFilename(PackageName));
	FEditedGrammar Edited(PackageName);
	if (!Edited.IsValid(*this) || !SaveCancelsInThisRun(*this))
	{
		return false;
	}
	Edited.DirtyPackage();
	Edited.Toolkit->SaveAsset_Execute();
	Edited.TestGrammarAndActorUnchanged(*this);
	TestFalse(TEXT("Nothing is written"), IFileManager::Get().FileExists(*PackageFilename(PackageName)));
	Edited.TestEditorKeepsTheEdits(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveAsOwnPathTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.SaveAsOntoTheGrammarsOwnPathSavesIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveAsOwnPathTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsOwnPath");
	const FString Filename = PackageFilename(PackageName);
	IFileManager::Get().Delete(*Filename);
	{
		const FEditedGrammar Edited(PackageName);
		if (!Edited.IsValid(*this))
		{
			return false;
		}
		int32 Asked = 0;
		AnswerSaveAs(*Edited.Toolkit, PackageName, Asked);

		RunWithoutSavePrompt([&Edited] { Edited.Toolkit->SaveAssetAs_Execute(); });
		TestEqual(TEXT("Save As asks for the asset once"), Asked, 1);
		TestEqualSensitive(TEXT("The grammar holds the edited text"), Edited.Grammar->GrammarText, FString(CountGrammarText));
		const FCambrianaParameterValue* Stored = FindStoredValue(*Edited.Grammar, TEXT("count"));
		TestTrue(TEXT("The grammar stores count = 5"), Stored && Stored->Value == 5.0);
		TestEqual(TEXT("The grammar holds 3 sides"), Edited.Grammar->Sides, 3);
		TestEqual(TEXT("The actor rebuilt once"), Edited.Actor->GetBuildCount(), Edited.ActorBuildsAfterEdits + 1);
		TestTrue(TEXT("The package is on disk"), IFileManager::Get().FileExists(*Filename));
		TestFalse(TEXT("The saved package is clean"), Edited.Grammar->GetPackage()->IsDirty());
		if (TestTrue(TEXT("The editor stays open on the grammar"), FindGrammarEditor(Edited.Grammar) == Edited.Toolkit))
		{
			TestFalse(TEXT("Nothing is left unsaved"), Edited.Toolkit->HasUnsavedEdits());
		}
	}
	DeleteSavedPackage(Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopySaveAsNewSaveCancelledTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.SaveAsWhoseNewAssetSaveDoesNotCompleteKeepsTheEditorOpenWithItsEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopySaveAsNewSaveCancelledTest::RunTest(const FString& Parameters)
{
	const TCHAR* NewPackageName = TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsNotSaved");
	const FString NewFilename = PackageFilename(NewPackageName);
	IFileManager::Get().Delete(*NewFilename);
	const FEditedGrammar Edited(TEXT("/Game/CambrianaSaveTests/WorkingCopySaveAsNotSavedOriginal"));
	if (!Edited.IsValid(*this) || !SaveCancelsInThisRun(*this))
	{
		return false;
	}
	Edited.Toolkit->SetSaveAsPrompt([NewPackageName](const UCambrianaGrammar&) { return FString(NewPackageName); });

	Edited.Toolkit->SaveAssetAs_Execute();
	Edited.TestGrammarAndActorUnchanged(*this);
	Edited.TestEditorKeepsTheEdits(*this);
	TestFalse(TEXT("Nothing is written for the new asset"), IFileManager::Get().FileExists(*NewFilename));
	UPackage* NewPackage = FindPackage(nullptr, NewPackageName);
	const UCambrianaGrammar* NewGrammar = NewPackage ? FindObject<UCambrianaGrammar>(NewPackage, *FPackageName::GetShortName(NewPackageName)) : nullptr;
	if (TestNotNull(TEXT("The new asset stays in memory, unsaved"), NewGrammar))
	{
		TestEqualSensitive(TEXT("The new asset holds the edited text"), NewGrammar->GrammarText, FString(CountGrammarText));
		TestTrue(TEXT("The new asset's package is dirty"), NewPackage->IsDirty());
		TestNull(TEXT("No editor opens on the new asset"), FindGrammarEditor(const_cast<UCambrianaGrammar*>(NewGrammar)));
	}
	if (NewPackage)
	{
		UnloadPackage(NewPackage);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyTabMarksDirtyPackageTest,
	"Cambriana.Editor.GrammarEditor.WorkingCopy.TheTabMarksADirtyPackageWithoutWorkingCopyEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyTabMarksDirtyPackageTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarInPackage(TEXT("/Game/CambrianaEditorTests/WorkingCopyDirtyPackage"));
	UPackage* Package = Grammar->GetPackage();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		UnloadPackage(Package);
		return false;
	}
	Package->SetDirtyFlag(true);
	TestFalse(TEXT("The editor has no unsaved edits"), Toolkit->HasUnsavedEdits());
	TestEqual(TEXT("The tab marks the dirty package"), Toolkit->GetTabSuffix().ToString(), FString(TEXT("*")));
	Package->SetDirtyFlag(false);
	TestTrue(TEXT("A clean package and no edits leave the tab unmarked"), Toolkit->GetTabSuffix().IsEmpty());
	CloseGrammarEditor(Grammar);
	UnloadPackage(Package);
	return true;
}

#endif
