#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CloseAsking;
using Cambriana::Editor::Tests::CloseGrammarEditor;
using Cambriana::Editor::Tests::CreateGrammarInPackage;
using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::FindGrammarEditor;
using Cambriana::Editor::Tests::OpenGrammarEditor;
using Cambriana::Editor::Tests::PackageFilename;
using Cambriana::Editor::Tests::TypedGrammarText;
using Cambriana::Editor::Tests::UnloadPackage;

// FAssetEditorToolkit::CloseWindow returns true whatever OnRequestClose answers, so the asset editor subsystem's closes
// (unloading or deleting the asset's package, consolidation, editor shutdown) carry on as though the editor had closed.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaClosePathNoCancelTest,
	"Cambriana.Editor.GrammarEditor.ClosePaths.ACloseTheEditorCannotStopOffersSaveOrDiscardOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaClosePathNoCancelTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));

	// Unattended, the editor's own dialog answers its default: Cancel when it offers Cancel, otherwise No.
	CloseAsking(Grammar);
	if (!TestNull(TEXT("Without a Cancel the unattended answer discards and the editor closes"), FindGrammarEditor(Grammar)))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, FString(UCambrianaGrammarFactory::DefaultGrammarText));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaClosePathSaveFailsTest,
	"Cambriana.Editor.GrammarEditor.ClosePaths.ASaveThatDoesNotCompleteAsksAgainWhenTheCloseCannotBeStopped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaClosePathSaveFailsTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/ClosePathSaveFails");
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
	Toolkit->CommitRulesText(FText::FromString(TypedGrammarText));
	int32 Asked = 0;
	TArray<EAppMsgType::Type> Offered;
	Toolkit->SetUnsavedEditsPrompt([&Asked, &Offered](EAppMsgType::Type Buttons, const FText&)
	{
		Offered.Add(Buttons);
		return ++Asked == 1 ? EAppReturnType::Yes : EAppReturnType::No;
	});

	// Unattended, the engine's save prompt cancels the save.
	CloseAsking(Grammar);
	TestEqual(TEXT("A save that does not complete asks again"), Asked, 2);
	TestTrue(TEXT("Each question offers save or discard only"), Offered.Num() == 2 && Offered[0] == EAppMsgType::YesNo && Offered[1] == EAppMsgType::YesNo);
	if (!TestNull(TEXT("Discarding at the second question closes the editor"), FindGrammarEditor(Grammar)))
	{
		CloseGrammarEditor(Grammar);
	}
	TestEqualSensitive(TEXT("The grammar keeps its text"), Grammar->GrammarText, FString(UCambrianaGrammarFactory::DefaultGrammarText));
	TestFalse(TEXT("Nothing was saved"), IFileManager::Get().FileExists(*Filename));
	UnloadPackage(Package);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaClosePathAskedOnceTest,
	"Cambriana.Editor.GrammarEditor.ClosePaths.EditsDiscardedWhenTheEditorIsAskedToCloseAreNotAskedAboutAgainWhenItsEditorsClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaClosePathAskedOnceTest::RunTest(const FString& Parameters)
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
	Toolkit->SetUnsavedEditsPrompt([&Asked](EAppMsgType::Type, const FText&) { ++Asked; return EAppReturnType::No; });

	// Shutting the editor down asks each tab whether it can close, then closes every asset editor.
	TestTrue(TEXT("Discarding lets the editor close"), Toolkit->OnRequestClose(EAssetEditorCloseReason::AssetEditorHostClosed));
	CloseAsking(Grammar);
	TestEqual(TEXT("The discarded edits are asked about once"), Asked, 1);
	if (!TestNull(TEXT("The editor closes"), FindGrammarEditor(Grammar)))
	{
		CloseGrammarEditor(Grammar);
	}
	return true;
}

#endif
