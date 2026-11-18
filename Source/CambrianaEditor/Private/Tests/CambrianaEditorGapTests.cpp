#include "ActorFactoryCambrianaGrammar.h"
#include "AssetDefinition_CambrianaGrammar.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarExporter.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarWorkingCopy.h"
#include "ScopedTransaction.h"
#include "CambrianaLiveActor.h"
#include "Editor.h"
#include "Exporters/Exporter.h"
#include "HAL/FileManager.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::DestroyActor;
using Cambriana::Editor::Tests::FGrammarChangeCounter;
using Cambriana::Editor::Tests::PlaceInCurrentLevel;
using Cambriana::Editor::Tests::MakeRows;

namespace
{
	FString GapTestPath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaGaps"), FileName));
	}

	/** Shuts a module down and starts it again when it goes out of scope, however the test ends. */
	class FModuleRestart
	{
	public:
		explicit FModuleRestart(IModuleInterface& InModule)
			: Module(InModule)
		{
			Module.ShutdownModule();
		}

		~FModuleRestart() { Module.StartupModule(); }

		FModuleRestart(const FModuleRestart&) = delete;
		FModuleRestart& operator=(const FModuleRestart&) = delete;

	private:
		IModuleInterface& Module;
	};

	/** Whether Details for a grammar shows a category of parameters, which only the grammar customization adds. */
	bool ShowsParameterCategory(UCambrianaGrammar* Grammar)
	{
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Grammar });
		return Rows->GetRootTreeNodes().ContainsByPredicate([](const TSharedRef<IDetailTreeNode>& Node) { return Node->GetNodeName() == TEXT("Structure Parameters"); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapExportBytesTest,
	"Cambriana.Editor.Export.ExportedFileHoldsTheTextsUtf8BytesExactly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapExportBytesTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = FString(TEXT("# naïve café\naxiom F\n"));
	const FString Path = GapTestPath(TEXT("bytes.lsys"));
	IFileManager::Get().Delete(*Path);
	TestTrue(TEXT("The export succeeds"), UExporter::ExportToFile(Grammar, nullptr, *Path, false) != 0);

	// "# na" 4 bytes, i-diaeresis 2, "ve caf" 6, e-acute 2, then "\naxiom F\n" 9 bytes: 23 bytes.
	TArray<uint8> Bytes;
	TestTrue(TEXT("The file is read"), FFileHelper::LoadFileToArray(Bytes, *Path));
	const TArray<uint8> Expected = { '#', ' ', 'n', 'a', 0xC3, 0xAF, 'v', 'e', ' ', 'c', 'a', 'f', 0xC3, 0xA9, '\n', 'a', 'x', 'i', 'o', 'm', ' ', 'F', '\n' };
	TestTrue(TEXT("The file holds exactly the text's UTF-8 bytes"), Bytes == Expected);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapExporterFormatTest,
	"Cambriana.Editor.Export.TheExporterOffersTheLsysFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapExporterFormatTest::RunTest(const FString& Parameters)
{
	const UCambrianaGrammarExporter* Exporter = GetDefault<UCambrianaGrammarExporter>();
	if (TestEqual(TEXT("One export format"), Exporter->FormatExtension.Num(), 1))
	{
		TestEqual(TEXT("Its extension is lsys"), Exporter->FormatExtension[0], FString(TEXT("lsys")));
	}
	TestTrue(TEXT("It is the preferred format"), Exporter->PreferredFormatIndex == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapActorFactoryTest,
	"Cambriana.Editor.ActorFactory.TheAssetOfAPlacedActorIsItsGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapActorFactoryTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	if (!TestNotNull(TEXT("An actor is placed from the grammar"), Actor))
	{
		return false;
	}
	UActorFactoryCambrianaGrammar* Factory = NewObject<UActorFactoryCambrianaGrammar>();
	TestTrue(TEXT("The factory gives back the actor's grammar"), Factory->GetAssetFromActorInstance(Actor) == Grammar);
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapAssetColourTest,
	"Cambriana.Editor.AssetDefinition.TheGrammarAssetHasTheEditorsGreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapAssetColourTest::RunTest(const FString& Parameters)
{
	// Regression pin: no spec gives the colour, and a grammar editor toolkit cannot be constructed outside an editor host to compare its tab colour.
	const FLinearColor Asset = GetDefault<UAssetDefinition_CambrianaGrammar>()->GetAssetColor();
	TestEqual(TEXT("The asset colour"), Asset.ToFColorSRGB(), FColor(64, 160, 96));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapUndoRebuildsActorsTest,
	"Cambriana.Editor.LiveActor.UndoingAGrammarChangeRebuildsItsActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapUndoRebuildsActorsTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	if (!TestNotNull(TEXT("An actor is placed from the grammar"), Actor))
	{
		return false;
	}
	// The undo history holds the grammar's edit alone: the change event that rebuilds the actor is sent outside it, so undoing the edit
	// does not restore the actor and only the grammar's own undo notification can rebuild it.
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Edit Sides")));
		Grammar->Modify();
		Grammar->Sides = 3;
	}
	Cambriana::Editor::NotifyGrammarChanged(*Grammar);
	TestEqual(TEXT("The edit rebuilt the actor with 3 sides"), Actor->GetLastBuildInput().Settings.Sides, 3);
	FGrammarChangeCounter Changes(Grammar);
	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestTrue(TEXT("Undo is broadcast as a change to the grammar"), Changes.Count >= 1);
	TestEqual(TEXT("Undo restored the grammar's 8 sides"), Grammar->Sides, 8);
	TestEqual(TEXT("Undo rebuilt the actor with 8 sides"), Actor->GetLastBuildInput().Settings.Sides, 8);
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapEditorModuleShutdownTest,
	"Cambriana.Editor.Module.ShutdownRemovesTheDetailsCustomizationsAndThePendingEngineInitStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapEditorModuleShutdownTest::RunTest(const FString& Parameters)
{
	IModuleInterface* Module = FModuleManager::Get().GetModule(TEXT("CambrianaEditor"));
	if (!TestNotNull(TEXT("The Cambriana editor module is loaded"), Module))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TEXT("structure param count = 3\n\naxiom repeat count { F }\n");
	TestTrue(TEXT("Details shows the parameters while the module runs"), ShowsParameterCategory(Grammar));

	// A start waiting for engine init, as the module leaves when it starts before the editor is up.
	const FDelegateHandle Pending = FCoreDelegates::GetOnPostEngineInit().AddRaw(Module, &IModuleInterface::StartupModule);
	{
		const FModuleRestart Restart(*Module);
		TestFalse(TEXT("Shutdown drops the module's pending engine init start"), FCoreDelegates::GetOnPostEngineInit().IsBoundToObject(Module));
		FCoreDelegates::GetOnPostEngineInit().Remove(Pending);
		TestFalse(TEXT("Details no longer shows the parameters after shutdown"), ShowsParameterCategory(Grammar));
	}
	TestTrue(TEXT("Details shows the parameters again after startup"), ShowsParameterCategory(Grammar));
	return true;
}

#endif
