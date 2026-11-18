#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarWorkingCopy.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::CopyGrammar;
using Cambriana::Editor::CopyGrammarValues;
using Cambriana::Editor::DuplicateAsAsset;
using Cambriana::Editor::DuplicateWorkingCopy;
using Cambriana::Editor::HoldSameGrammar;
using Cambriana::Editor::Tests::FGrammarChangeCounter;
using Cambriana::Editor::Tests::UndoHistoryLength;
using Cambriana::Editor::NotifyGrammarChanged;
using Cambriana::Editor::Tests::CreateGrammarInPackage;
using Cambriana::Editor::Tests::UnloadPackage;

namespace
{
	UCambrianaGrammar* TransientGrammar(const TCHAR* Text, uint32 Seed = 1)
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage(), NAME_None, RF_Transactional);
		Grammar->GrammarText = Text;
		Grammar->Seed = Seed;
		return Grammar;
	}

	FCambrianaParameterValue Stored(const TCHAR* Name, double Value)
	{
		FCambrianaParameterValue Parameter;
		Parameter.Name = Name;
		Parameter.Value = Value;
		return Parameter;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyDuplicateTest,
	"Cambriana.Editor.WorkingCopy.ADuplicateIsTransientTransactionalAndHoldsTheGrammarExceptItsImportData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyDuplicateTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Source = NewObject<UCambrianaGrammar>(GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);
	Source->GrammarText = TEXT("axiom F\n");
	Source->Seed = 9;
	Source->Sides = 5;
	Source->ParameterValues.Add(Stored(TEXT("count"), 4.0));
	Source->AssetImportData->Update(TEXT("source.lsys"));

	UCambrianaGrammar* Copy = DuplicateWorkingCopy(*Source);
	if (!TestNotNull(TEXT("A duplicate is made"), Copy))
	{
		Source->ClearFlags(RF_Standalone);
		return false;
	}
	TestFalse(TEXT("The duplicate is not public"), Copy->HasAnyFlags(RF_Public));
	TestFalse(TEXT("The duplicate is not standalone"), Copy->HasAnyFlags(RF_Standalone));
	TestTrue(TEXT("The duplicate is transactional although its source is not"), Copy->HasAnyFlags(RF_Transactional));
	TestTrue(TEXT("The duplicate is in the transient package"), Copy->GetOutermost() == GetTransientPackage());
	TestEqualSensitive(TEXT("The duplicate holds the text"), Copy->GrammarText, Source->GrammarText);
	TestEqual(TEXT("The duplicate holds the seed"), Copy->Seed, 9u);
	TestEqual(TEXT("The duplicate holds the sides"), Copy->Sides, 5);
	TestEqual(TEXT("The duplicate holds the parameter values"), Copy->ParameterValues.Num(), 1);
	TestTrue(TEXT("The duplicate's import data is its own object"), Copy->AssetImportData != Source->AssetImportData);
	TestTrue(TEXT("A grammar and its duplicate hold the same grammar although their import data are different objects"), HoldSameGrammar(*Source, *Copy));
	Source->ClearFlags(RF_Standalone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyHoldSameTest,
	"Cambriana.Editor.WorkingCopy.GrammarsAreTheSameOnlyWhenTextNamesAndValuesMatchIncludingCase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyHoldSameTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* A = TransientGrammar(TEXT("axiom F\n"));
	UCambrianaGrammar* B = TransientGrammar(TEXT("axiom F\n"));
	A->ParameterValues.Add(Stored(TEXT("count"), 5.0));
	B->ParameterValues.Add(Stored(TEXT("count"), 5.0));
	TestTrue(TEXT("Equal grammars hold the same grammar"), HoldSameGrammar(*A, *B));

	B->ParameterValues[0].Value = 6.0;
	TestFalse(TEXT("A different parameter value is a different grammar"), HoldSameGrammar(*A, *B));
	B->ParameterValues[0].Value = 5.0;
	B->ParameterValues[0].Name = TEXT("Count");
	TestFalse(TEXT("A parameter name that differs only in case is a different grammar"), HoldSameGrammar(*A, *B));
	B->ParameterValues[0].Name = TEXT("count");
	B->GrammarText = TEXT("axiom f\n");
	TestFalse(TEXT("Text that differs only in case is a different grammar"), HoldSameGrammar(*A, *B));
	B->GrammarText = A->GrammarText;
	B->Seed = 2;
	TestFalse(TEXT("A different seed is a different grammar"), HoldSameGrammar(*A, *B));
	return true;
}

namespace
{
	UCambrianaGrammar* CopySource()
	{
		UCambrianaGrammar* From = TransientGrammar(TEXT("axiom F F\n"), 5);
		From->Sides = 3;
		From->ParameterValues.Add(Stored(TEXT("count"), 2.0));
		return From;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCopyDirtiesTest,
	"Cambriana.Editor.WorkingCopy.CopyingADifferentGrammarChangesTheTargetAndDirtiesItsPackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCopyDirtiesTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* To = CreateGrammarInPackage(TEXT("/Game/CambrianaWorkingCopyTests/CopyTarget"));
	UCambrianaGrammar* From = CopySource();
	UPackage* Package = To->GetPackage();
	Package->SetDirtyFlag(false);

	TestTrue(TEXT("Copying a different grammar changes the target"), CopyGrammar(*From, *To));
	TestTrue(TEXT("The target holds the copied grammar"), HoldSameGrammar(*From, *To));
	TestTrue(TEXT("The target's package is dirty"), Package->IsDirty());
	TestFalse(TEXT("Copying the same grammar changes nothing"), CopyGrammar(*From, *To));
	TestFalse(TEXT("Copying values of the same grammar changes nothing"), CopyGrammarValues(*From, *To));
	UnloadPackage(Package);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCopyBroadcastTest,
	"Cambriana.Editor.WorkingCopy.ACopyBroadcastsOneChangeAndCopyingTheSameGrammarBroadcastsNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCopyBroadcastTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* To = TransientGrammar(TEXT("axiom F\n"));
	UCambrianaGrammar* From = CopySource();
	FGrammarChangeCounter Listener(nullptr, true);
	TestTrue(TEXT("Copying a different grammar changes the target"), CopyGrammar(*From, *To));
	TestEqual(TEXT("The change is broadcast once"), Listener.Count, 1);
	TestFalse(TEXT("Copying the same grammar again changes nothing"), CopyGrammar(*From, *To));
	TestEqual(TEXT("and broadcasts nothing"), Listener.Count, 1);
	NotifyGrammarChanged(*To);
	TestEqual(TEXT("A change notification is one more broadcast"), Listener.Count, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyCopyUndoTest,
	"Cambriana.Editor.WorkingCopy.ACopyAndItsListenersWriteNothingIntoTheUndoHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyCopyUndoTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* To = TransientGrammar(TEXT("axiom F\n"));
	UCambrianaGrammar* From = CopySource();
	FGrammarChangeCounter Listener(nullptr, true);
	const int32 HistoryBefore = UndoHistoryLength();
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Copy")));
		TestTrue(TEXT("Copying inside a transaction changes the target"), CopyGrammar(*From, *To));
	}
	TestFalse(TEXT("The change is broadcast outside the undo transaction"), Listener.bUndoActive);
	TestEqual(TEXT("A copy and its listeners write nothing into the undo history"), UndoHistoryLength(), HistoryBefore);

	From->Seed = 6;
	Listener.bUndoActive = false;
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Notify")));
		NotifyGrammarChanged(*To);
	}
	TestEqual(TEXT("A change notification is broadcast"), Listener.Count, 2);
	TestFalse(TEXT("outside the undo transaction"), Listener.bUndoActive);
	TestEqual(TEXT("and its listeners write nothing into the undo history"), UndoHistoryLength(), HistoryBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaWorkingCopyDuplicateAsAssetTest,
	"Cambriana.Editor.WorkingCopy.AnAssetDuplicateIsStandaloneDirtyAndInTheAssetRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaWorkingCopyDuplicateAsAssetTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaWorkingCopyTests/AsAsset");
	UCambrianaGrammar* Source = TransientGrammar(TEXT("axiom F F\n"), 5);
	TArray<const UObject*> Announced;
	const FDelegateHandle Handle = IAssetRegistry::GetChecked().OnInMemoryAssetCreated().AddLambda([&Announced](UObject* Created) { Announced.Add(Created); });
	UCambrianaGrammar* Asset = DuplicateAsAsset(*Source, PackageName);
	IAssetRegistry::GetChecked().OnInMemoryAssetCreated().Remove(Handle);
	if (!TestNotNull(TEXT("An asset is made"), Asset))
	{
		return false;
	}
	TestTrue(TEXT("The asset is public, standalone and transactional"), Asset->HasAllFlags(RF_Public | RF_Standalone | RF_Transactional));
	TestFalse(TEXT("The asset is not transient"), Asset->HasAnyFlags(RF_Transient));
	TestTrue(TEXT("The asset's package is dirty"), Asset->GetPackage()->IsDirty());
	TestEqualSensitive(TEXT("The asset holds the text"), Asset->GrammarText, Source->GrammarText);
	TestEqual(TEXT("The asset holds the seed"), Asset->Seed, 5u);
	TestTrue(TEXT("The asset registry was told once that the asset was created"), Announced.Num() == 1 && Announced[0] == Asset);
	UnloadPackage(Asset->GetPackage());
	return true;
}

#endif
