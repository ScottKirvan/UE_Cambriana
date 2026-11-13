#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CloseFromTab;
using Cambriana::Editor::Tests::CloseGrammarEditor;
using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::EditGrammarSides;
using Cambriana::Editor::Tests::FindGrammarEditor;
using Cambriana::Editor::Tests::OpenGrammarEditor;
using Cambriana::Editor::Tests::PauseTyping;
using Cambriana::Editor::Tests::RulesTextBoxOf;
using Cambriana::Editor::Tests::TypedGrammarText;
using Cambriana::Editor::Tests::TypeOverRules;

namespace
{
	const TCHAR* const CountText = TEXT("structure param count = 3\n\naxiom repeat count { F }\n");

	/** A grammar editor on a new grammar that is not an asset; closed with its edits discarded. */
	struct FOpenEditor
	{
		UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
		FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);

		~FOpenEditor() { CloseGrammarEditor(Grammar); }

		FOpenEditor() = default;
		FOpenEditor(const FOpenEditor&) = delete;
		FOpenEditor& operator=(const FOpenEditor&) = delete;

		bool IsValid(FAutomationTestBase& Test) const { return Test.TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit); }
	};

	/** Types Text over the whole Rules text in one insertion. */
	void TypeOtherText(SMultiLineEditableTextBox& Rules, const TCHAR* Text)
	{
		Rules.SelectAllText();
		Rules.InsertTextAtCursor(FString(Text));
	}

	int32 UndoHistoryLength()
	{
		return GEditor->Trans->GetQueueLength();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitCloseAppliesTypingTest,
	"Cambriana.Editor.GrammarEditorGates.ClosingAppliesTheTypedTextBeforeAskingAndAsksOnceForUnsavedEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitCloseAppliesTypingTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	TypeOverRules(*RulesTextBoxOf(*Editor.Toolkit));
	int32 Asked = 0;
	Editor.Toolkit->SetUnsavedEditsPrompt([&Asked](EAppMsgType::Type, const FText&) { ++Asked; return EAppReturnType::Cancel; });
	TestFalse(TEXT("Cancelling keeps the editor open"), CloseFromTab(*Editor.Toolkit));
	TestEqual(TEXT("Closing asked once"), Asked, 1);
	TestEqualSensitive(TEXT("The typed text reached the working copy before the question"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitCloseReasonsTest,
	"Cambriana.Editor.GrammarEditorGates.AForceDeleteDoesNotAskAndACloseWithoutACancelButtonDiscardsEvenWhenThePromptCancels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitCloseReasonsTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	Editor.Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	int32 Asked = 0;
	EAppMsgType::Type Offered = EAppMsgType::Ok;
	Editor.Toolkit->SetUnsavedEditsPrompt([&Asked, &Offered](EAppMsgType::Type Buttons, const FText&) { ++Asked; Offered = Buttons; return EAppReturnType::Cancel; });

	TestTrue(TEXT("A force delete closes"), Editor.Toolkit->OnRequestClose(EAssetEditorCloseReason::AssetForceDeleted));
	TestEqual(TEXT("A force delete does not ask"), Asked, 0);

	TestTrue(TEXT("A close that cannot be stopped closes although the prompt cancels"), Editor.Toolkit->OnRequestClose(EAssetEditorCloseReason::CloseAllAssetEditors));
	TestEqual(TEXT("It asked once"), Asked, 1);
	TestTrue(TEXT("offering Yes and No only"), Offered == EAppMsgType::YesNo);
	TestTrue(TEXT("A second request to close after discarding closes"), Editor.Toolkit->OnRequestClose(EAssetEditorCloseReason::CloseAllAssetEditors));
	TestEqual(TEXT("Discarded edits are not asked about again"), Asked, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitDiscardedAskAgainTest,
	"Cambriana.Editor.GrammarEditorGates.AnEditAfterDiscardingAtTheCloseQuestionIsAskedAboutAgain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitDiscardedAskAgainTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	int32 Asked = 0;
	Editor.Toolkit->SetUnsavedEditsPrompt([&Asked](EAppMsgType::Type, const FText&) { ++Asked; return EAppReturnType::No; });
	Editor.Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	TestTrue(TEXT("Discarding closes"), Editor.Toolkit->OnRequestClose(EAssetEditorCloseReason::AssetEditorHostClosed));
	TestEqual(TEXT("The first close asked"), Asked, 1);
	TestTrue(TEXT("A second close request closes"), Editor.Toolkit->OnRequestClose(EAssetEditorCloseReason::AssetEditorHostClosed));
	TestEqual(TEXT("without asking again"), Asked, 1);

	Editor.Toolkit->CommitRulesText(FText::FromString(CountText));
	TestTrue(TEXT("Discarding the new edit closes"), Editor.Toolkit->OnRequestClose(EAssetEditorCloseReason::AssetEditorHostClosed));
	TestEqual(TEXT("A new edit is asked about again"), Asked, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitSaveAsAppliesTypingTest,
	"Cambriana.Editor.GrammarEditorGates.SaveAsAppliesTheTypedTextBeforeAskingForTheName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitSaveAsAppliesTypingTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	TypeOverRules(*RulesTextBoxOf(*Editor.Toolkit));
	bool bApplied = false;
	Editor.Toolkit->SetSaveAsPrompt([&bApplied, Toolkit = Editor.Toolkit](const UCambrianaGrammar&)
	{
		bApplied = Toolkit->GetWorkingCopy()->GrammarText.Equals(TypedGrammarText, ESearchCase::CaseSensitive);
		return FString();
	});
	Editor.Toolkit->SaveAssetAs_Execute();
	TestTrue(TEXT("The working copy held the typed text when Save As asked for the name"), bApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitSaveAsInvalidNameTest,
	"Cambriana.Editor.GrammarEditorGates.SaveAsWithAnInvalidAssetNameMakesNoAssetAndKeepsTheEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitSaveAsInvalidNameTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	Editor.Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	const TCHAR* InvalidName = TEXT("/Game/CambrianaSaveTests/CON");
	Editor.Toolkit->SetSaveAsPrompt([InvalidName](const UCambrianaGrammar&) { return FString(InvalidName); });
	Editor.Toolkit->SaveAssetAs_Execute();
	TestNull(TEXT("No package is made for the invalid name"), FindPackage(nullptr, InvalidName));
	TestTrue(TEXT("The editor stays open"), FindGrammarEditor(Editor.Grammar) == Editor.Toolkit);
	TestTrue(TEXT("The edits are still unsaved"), Editor.Toolkit->HasUnsavedEdits());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitCommitRulesTest,
	"Cambriana.Editor.GrammarEditorGates.CommittingTheSameTextChangesNothingAndTextDifferingInCaseIsAChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitCommitRulesTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	const FString Before = Editor.Toolkit->GetWorkingCopy()->GrammarText;
	int32 Changes = 0;
	const UCambrianaGrammar* WorkingCopy = Editor.Toolkit->GetWorkingCopy();
	const FDelegateHandle Handle = UCambrianaGrammar::OnGrammarChanged().AddLambda([&Changes, WorkingCopy](const UCambrianaGrammar* Changed) { Changes += Changed == WorkingCopy; });
	Editor.Toolkit->CommitRulesText(FText::FromString(Before));
	TestEqual(TEXT("Committing the working copy's own text is not a change to it"), Changes, 0);
	TestFalse(TEXT("and leaves nothing unsaved"), Editor.Toolkit->HasUnsavedEdits());

	const FString Shouted = Before.Replace(TEXT("A"), TEXT("a"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("The test text differs in case"), Shouted.Equals(Before, ESearchCase::CaseSensitive));
	Editor.Toolkit->CommitRulesText(FText::FromString(Shouted));
	TestEqualSensitive(TEXT("Text that differs only in case reaches the working copy"), Editor.Toolkit->GetWorkingCopy()->GrammarText, Shouted);
	TestEqualSensitive(TEXT("and the Rules text"), Editor.Toolkit->GetRulesText().ToString(), Shouted);
	TestTrue(TEXT("and is an unsaved edit"), Editor.Toolkit->HasUnsavedEdits());
	TestEqual(TEXT("and is one change to the working copy"), Changes, 1);
	UCambrianaGrammar::OnGrammarChanged().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitGrammarChangeDuringTypingTest,
	"Cambriana.Editor.GrammarEditorGates.AGrammarChangeFromElsewhereDuringTheTypingPauseDoesNotReachTheWorkingCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitGrammarChangeDuringTypingTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	TypeOverRules(*RulesTextBoxOf(*Editor.Toolkit));
	EditGrammarSides(Editor.Grammar, 3);
	TestEqual(TEXT("With a Rules edit pending the working copy keeps its 8 sides"), Editor.Toolkit->GetWorkingCopy()->Sides, 8);
	PauseTyping(Editor.Toolkit->RulesApplyDelaySeconds);
	TestEqualSensitive(TEXT("The typed text still reaches the working copy"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("which still has 8 sides"), Editor.Toolkit->GetWorkingCopy()->Sides, 8);
	TestTrue(TEXT("and the edits are unsaved"), Editor.Toolkit->HasUnsavedEdits());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitTypingTimerTest,
	"Cambriana.Editor.GrammarEditorGates.TypingAgainRestartsThePause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitTypingTimerTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	const TSharedPtr<SMultiLineEditableTextBox> Rules = RulesTextBoxOf(*Editor.Toolkit);
	const FString Default = Editor.Toolkit->GetWorkingCopy()->GrammarText;
	const float Delay = Editor.Toolkit->RulesApplyDelaySeconds;

	TypeOverRules(*Rules);
	PauseTyping(Delay * 0.6f);
	TypeOverRules(*Rules);
	PauseTyping(Delay * 0.6f);
	TestEqualSensitive(TEXT("Typing again restarts the pause, so 0.6 pauses after the first typing nothing has been applied"),
		Editor.Toolkit->GetWorkingCopy()->GrammarText, Default);
	PauseTyping(Delay * 0.6f);
	TestEqualSensitive(TEXT("A pause after the last typing applies it"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitPauseAppliesOnceTest,
	"Cambriana.Editor.GrammarEditorGates.APauseAppliesItsTypingOnceAndDoesNotApplyTheNextTypingEarly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitPauseAppliesOnceTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	const TSharedPtr<SMultiLineEditableTextBox> Rules = RulesTextBoxOf(*Editor.Toolkit);
	const float Delay = Editor.Toolkit->RulesApplyDelaySeconds;

	int32 Changes = 0;
	const UCambrianaGrammar* WorkingCopy = Editor.Toolkit->GetWorkingCopy();
	const FDelegateHandle Handle = UCambrianaGrammar::OnGrammarChanged().AddLambda([&Changes, WorkingCopy](const UCambrianaGrammar* Changed) { Changes += Changed == WorkingCopy; });
	TypeOverRules(*Rules);
	PauseTyping(Delay);
	TestEqualSensitive(TEXT("A pause applies the typing"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestEqual(TEXT("as one change"), Changes, 1);
	PauseTyping(Delay);
	TestEqual(TEXT("and a second pause with no typing applies nothing more"), Changes, 1);

	PauseTyping(Delay * 0.2f);
	TypeOtherText(*Rules, CountText);
	PauseTyping(Delay * 0.9f);
	TestEqualSensitive(TEXT("The pause that applied the earlier typing does not apply typing made 0.9 pauses ago"),
		Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	TestTrue(TEXT("The new typing is pending"), Editor.Toolkit->HasUnsavedEdits());
	PauseTyping(Delay * 0.3f);
	TestEqualSensitive(TEXT("It applies a pause after it was typed"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(CountText));
	TestEqual(TEXT("as the second change"), Changes, 2);
	UCambrianaGrammar::OnGrammarChanged().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitCommitCancelsTypingTimerTest,
	"Cambriana.Editor.GrammarEditorGates.CommittingTextEndsThePendingPauseSoItDoesNotApplyLaterTyping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitCommitCancelsTypingTimerTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	const TSharedPtr<SMultiLineEditableTextBox> Rules = RulesTextBoxOf(*Editor.Toolkit);
	const float Delay = Editor.Toolkit->RulesApplyDelaySeconds;

	TypeOtherText(*Rules, TEXT("axiom F F\n"));
	PauseTyping(Delay * 0.6f);
	Editor.Toolkit->CommitRulesText(FText::FromString(CountText));
	TypeOverRules(*Rules);
	PauseTyping(Delay * 0.6f);
	TestEqualSensitive(TEXT("Typing after a commit waits for its own pause"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(CountText));
	TestTrue(TEXT("and is pending meanwhile"), Editor.Toolkit->HasUnsavedEdits());
	PauseTyping(Delay * 0.6f);
	TestEqualSensitive(TEXT("It applies a pause after it was typed"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));

	// A commit leaves no timer that can fire later: with no further typing a full pause passes and nothing more is applied.
	int32 Changes = 0;
	const UCambrianaGrammar* WorkingCopy = Editor.Toolkit->GetWorkingCopy();
	const FDelegateHandle Handle = UCambrianaGrammar::OnGrammarChanged().AddLambda([&Changes, WorkingCopy](const UCambrianaGrammar* Changed) { Changes += Changed == WorkingCopy; });
	TypeOtherText(*Rules, TEXT("axiom F F\n"));
	PauseTyping(Delay * 0.6f);
	Editor.Toolkit->CommitRulesText(FText::FromString(CountText));
	const int32 ChangesAfterCommit = Changes;
	PauseTyping(Delay);
	UCambrianaGrammar::OnGrammarChanged().Remove(Handle);
	TestEqualSensitive(TEXT("After a commit a full pause leaves the committed text"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(CountText));
	TestEqual(TEXT("and sends no further change"), Changes, ChangesAfterCommit);
	TestEqual(TEXT("the commit itself being one change"), ChangesAfterCommit, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitSaveWithoutAssetTest,
	"Cambriana.Editor.GrammarEditorGates.SavingAGrammarThatIsNotAnAssetCopiesTheEditsWithoutSavingAPackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitSaveWithoutAssetTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	TestFalse(TEXT("The new grammar is not an asset"), Editor.Grammar->IsAsset());
	Editor.Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	Editor.Toolkit->SaveAsset_Execute();
	TestEqualSensitive(TEXT("The grammar holds the edited text"), Editor.Grammar->GrammarText, FString(TypedGrammarText));
	TestFalse(TEXT("Nothing is left unsaved"), Editor.Toolkit->HasUnsavedEdits());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaToolkitUndoRedoCommandsTest,
	"Cambriana.Editor.GrammarEditorGates.UndoAndRedoCommandsFirstApplyWhatIsBeingTyped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaToolkitUndoRedoCommandsTest::RunTest(const FString& Parameters)
{
	const FOpenEditor Editor;
	if (!Editor.IsValid(*this))
	{
		return false;
	}
	const TSharedRef<FUICommandList> Commands = Editor.Toolkit->GetToolkitCommands();
	const FString Default = Editor.Toolkit->GetWorkingCopy()->GrammarText;
	const TSharedPtr<SMultiLineEditableTextBox> Rules = RulesTextBoxOf(*Editor.Toolkit);

	Editor.Toolkit->CommitRulesText(FText::FromString(CountText));
	TypeOverRules(*Rules);
	Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef());
	TestEqualSensitive(TEXT("Undo while typing applies the typing, then undoes it"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(CountText));

	Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef());
	TestEqualSensitive(TEXT("A second undo undoes the first commit"), Editor.Toolkit->GetWorkingCopy()->GrammarText, Default);

	TypeOverRules(*Rules);
	Commands->ExecuteAction(FGenericCommands::Get().Redo.ToSharedRef());
	TestEqualSensitive(TEXT("Redo while typing applies the typing, which ends the redo history"), Editor.Toolkit->GetWorkingCopy()->GrammarText, FString(TypedGrammarText));
	return true;
}

#endif
