#include "ActorFactoryCambrianaGrammar.h"
#include "AssetDefinition_CambrianaGrammar.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarExporter.h"
#include "CambrianaGrammarFactory.h"
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
using Cambriana::Editor::Tests::EditGrammarSides;
using Cambriana::Editor::Tests::PlaceInCurrentLevel;

namespace
{
	FString GapTestPath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaGaps"), FileName));
	}

	/** Whether Details for a grammar shows a category of parameters, which only the grammar customization adds. */
	bool ShowsParameterCategory(UCambrianaGrammar* Grammar)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		const TSharedRef<IPropertyRowGenerator> Rows = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Rows->SetObjects({ Grammar });
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
	Actor->GetWorld()->EditorDestroyActor(Actor, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGapAssetColourTest,
	"Cambriana.Editor.AssetDefinition.TheGrammarAssetHasTheEditorsGreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGapAssetColourTest::RunTest(const FString& Parameters)
{
	// Regression pin: no spec gives the colour; the asset's and the editor tab's are the same green (64, 160, 96).
	const FLinearColor Asset = GetDefault<UAssetDefinition_CambrianaGrammar>()->GetAssetColor();
	TestEqual(TEXT("The asset colour"), Asset.ToFColorSRGB(), FColor(64, 160, 96));
	TestEqual(TEXT("It is the editor tab's colour"), Asset, FLinearColor(FColor(64, 160, 96)));
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
	EditGrammarSides(Grammar, 3);
	TestEqual(TEXT("The edit rebuilt the actor with 3 sides"), Actor->GetLastBuildInput().Settings.Sides, 3);
	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restored the grammar's 8 sides"), Grammar->Sides, 8);
	TestEqual(TEXT("Undo rebuilt the actor with 8 sides"), Actor->GetLastBuildInput().Settings.Sides, 8);
	Actor->GetWorld()->EditorDestroyActor(Actor, false);
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
	Module->ShutdownModule();
	TestFalse(TEXT("Shutdown drops the module's pending engine init start"), FCoreDelegates::GetOnPostEngineInit().IsBoundToObject(Module));
	FCoreDelegates::GetOnPostEngineInit().Remove(Pending);
	TestFalse(TEXT("Details no longer shows the parameters after shutdown"), ShowsParameterCategory(Grammar));

	Module->StartupModule();
	TestTrue(TEXT("Details shows the parameters again after startup"), ShowsParameterCategory(Grammar));
	return true;
}

#endif
