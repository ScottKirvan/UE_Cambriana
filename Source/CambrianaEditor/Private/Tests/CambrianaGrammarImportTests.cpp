#include "ScopedTransaction.h"
#include "Misc/SecureHash.h"
#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "AssetImportTask.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFormat.h"
#include "CambrianaGrammarPreview.h"
#include "CambrianaLiveActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IAssetTypeActions.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	const TCHAR* ImportDestination = TEXT("/Game/CambrianaImportTests");

	class FImportedGrammar
	{
	public:
		explicit FImportedGrammar(const FString& Filename)
		{
			static int32 ImportCount = 0;
			Task = NewObject<UAssetImportTask>();
			Task->AddToRoot();
			Task->Filename = Filename;
			Task->DestinationPath = ImportDestination;
			Task->DestinationName = FString::Printf(TEXT("%s_Import%d"), *FPaths::GetBaseFilename(Filename), ++ImportCount);
			Task->bAutomated = true;
			Task->bSave = false;
			Task->bReplaceExisting = true;
			FAssetToolsModule::GetModule().Get().ImportAssetTasks({ Task });
		}

		~FImportedGrammar()
		{
			TArray<UObject*> Objects = Task->GetObjects();
			if (!Objects.IsEmpty())
			{
				ObjectTools::ForceDeleteObjects(Objects, false);
			}
			Task->RemoveFromRoot();
		}

		FImportedGrammar(const FImportedGrammar&) = delete;
		FImportedGrammar& operator=(const FImportedGrammar&) = delete;

		int32 NumObjects() const { return Task->GetObjects().Num(); }

		UCambrianaGrammar* Grammar() const
		{
			const TArray<UObject*>& Objects = Task->GetObjects();
			return Objects.Num() == 1 ? Cast<UCambrianaGrammar>(Objects[0]) : nullptr;
		}

		FString ObjectPath() const
		{
			return FString::Printf(TEXT("%s/%s.%s"), ImportDestination, *Task->DestinationName, *Task->DestinationName);
		}

	private:
		UAssetImportTask* Task = nullptr;
	};

	FString WriteTemporaryFile(const TCHAR* FileName, const FString& Text)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaImport"), FileName));
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		return Path;
	}

	/** Deletes the files it is given when it goes out of scope, however the test ends. */
	class FTempFiles
	{
	public:
		FTempFiles(std::initializer_list<FString> InPaths)
			: Paths(InPaths)
		{
		}

		~FTempFiles()
		{
			for (const FString& Path : Paths)
			{
				IFileManager::Get().Delete(*Path);
			}
		}

		FTempFiles(const FTempFiles&) = delete;
		FTempFiles& operator=(const FTempFiles&) = delete;

	private:
		TArray<FString> Paths;
	};

	UAssetImportData* ImportDataOf(const UObject* Object)
	{
		const FObjectProperty* Property = CastField<FObjectProperty>(Object->GetClass()->FindPropertyByName(TEXT("AssetImportData")));
		return Property ? Cast<UAssetImportData>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportLegacyBuildsSameBonesTest,
	"Cambriana.Editor.Import.LegacyFileBecomesLsysThatBuildsTheSameBones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportLegacyBuildsSameBonesTest::RunTest(const FString& Parameters)
{
	const FString Path = FixturePath(TEXT("sk.ls"));
	const FImportedGrammar Imported(Path);
	const UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing sk.ls creates a grammar asset"), Grammar))
	{
		return false;
	}
	TestTrue(TEXT("The asset holds .lsys text"), Cambriana::DetectGrammarFormat(Grammar->GrammarText) == ECambrianaGrammarFormat::Cambriana);

	const FCoreResult Legacy(ECambrianaGrammarFormat::Legacy, LoadFile(Path), 12345);
	const FCoreResult Converted(ECambrianaGrammarFormat::Cambriana, Grammar->GrammarText, 12345);
	if (!TestTrue(TEXT("Legacy file built"), Legacy.IsValid()) || !TestTrue(TEXT("Asset's grammar built"), Converted.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("Legacy file has no errors"), Legacy.HasErrors());
	TestFalse(TEXT("Asset's grammar has no errors"), Converted.HasErrors());

	const TConstArrayView<cambriana_bone> LegacyBones = Legacy.Bones();
	const TConstArrayView<cambriana_bone> ConvertedBones = Converted.Bones();
	TestTrue(TEXT("Legacy file has bones"), LegacyBones.Num() > 0);
	if (!TestEqual(TEXT("Bone count"), ConvertedBones.Num(), LegacyBones.Num()))
	{
		return false;
	}
	TestEqual(TEXT("Bones that differ from the legacy file's"), CountBoneMismatches(ConvertedBones, LegacyBones), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportLsysStoresTextUnchangedTest,
	"Cambriana.Editor.Import.LsysFileTextIsStoredUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportLsysStoresTextUnchangedTest::RunTest(const FString& Parameters)
{
	const FString Path = FixturePath(TEXT("sk.lsys"));
	const FImportedGrammar Imported(Path);
	const UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing sk.lsys creates a grammar asset"), Grammar))
	{
		return false;
	}
	const FString FileText = LoadFile(Path);
	TestFalse(TEXT("The fixture has text"), FileText.IsEmpty());
	TestEqualSensitive(TEXT("The asset holds the file's text"), Grammar->GrammarText, FileText);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportLsysSeedTest,
	"Cambriana.Editor.Import.LsysSeedSettingBecomesTheAssetSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportLsysSeedTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("seeded.lsys"), TEXT("generations 3\nseed 7\naxiom A\n\ngrow {\n    A -> F A\n}\n"));
	const FImportedGrammar Imported(Path);
	const UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing a seeded .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	TestEqual(TEXT("The asset's seed is the file's seed"), Grammar->Seed, 7u);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportRejectedLegacyTest,
	"Cambriana.Editor.Import.RejectedLegacyFileReportsLineAndColumnAndCreatesNoAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportRejectedLegacyTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("rejected.ls"), TEXT("3\n25\n50\nA\nA=F@F@\n@\n"));
	AddExpectedMessagePlain(TEXT("rejected.ls: error at line 5, column 4: not convertible"), ELogVerbosity::Error);
	AddExpectedMessagePlain(TEXT("rejected.ls: error at line 5, column 6: not convertible"), ELogVerbosity::Error);
	AddExpectedMessagePlain(TEXT("Failed to import"), ELogVerbosity::Warning);
	const FImportedGrammar Imported(Path);
	TestEqual(TEXT("The import returns no objects"), Imported.NumObjects(), 0);
	TestNull(TEXT("No grammar asset exists at the destination"), FindObject<UCambrianaGrammar>(nullptr, *Imported.ObjectPath()));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportRecordsSourcePathTest,
	"Cambriana.Editor.Import.AssetRecordsItsSourceFilePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportRecordsSourcePathTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* FileName : { TEXT("sk.ls"), TEXT("sk.lsys") })
	{
		const FString Path = FixturePath(FileName);
		const FImportedGrammar Imported(Path);
		const UCambrianaGrammar* Grammar = Imported.Grammar();
		if (!TestNotNull(FString::Printf(TEXT("Importing %s creates a grammar asset"), FileName), Grammar))
		{
			continue;
		}
		const UAssetImportData* ImportData = ImportDataOf(Grammar);
		if (TestNotNull(TEXT("The asset has import data"), ImportData))
		{
			TestTrue(FString::Printf(TEXT("The asset records %s as its source"), FileName), FPaths::IsSamePath(ImportData->GetFirstFilename(), Path));
		}
	}
	return true;
}

namespace
{
	bool Reimport(UCambrianaGrammar* Grammar)
	{
		return FReimportManager::Instance()->CanReimport(Grammar)
			&& FReimportManager::Instance()->Reimport(Grammar, false, false, FString(), nullptr, INDEX_NONE, false, true);
	}

	const TCHAR* FirstLegacyText = TEXT("3\n25\n50\nA\nA=+F'A\n@\n");
	const TCHAR* SecondLegacyText = TEXT("4\n30\n50\nc[A]c[B]\nA=+F'A\nB=-F'B\n@\n");
	const TCHAR* FirstLsysText = TEXT("generations 3\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	const TCHAR* SecondLsysText = TEXT("generations 4\naxiom A\n\ngrow {\n    A -> F [ + A ] A\n}\n");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportLsysReplacesTextTest,
	"Cambriana.Editor.Reimport.LsysFileTextReplacesTheAssetText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportLsysReplacesTextTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport.lsys"), FirstLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	TestEqualSensitive(TEXT("The asset holds the first text"), Grammar->GrammarText, FString(FirstLsysText));

	WriteTemporaryFile(TEXT("reimport.lsys"), SecondLsysText);
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestEqualSensitive(TEXT("The asset holds the changed file's text"), Grammar->GrammarText, FString(SecondLsysText));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportLegacyConvertsAgainTest,
	"Cambriana.Editor.Reimport.LegacyFileIsConvertedAgainAndBuildsItsNewBones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportLegacyConvertsAgainTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport.ls"), FirstLegacyText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .ls file creates a grammar asset"), Grammar))
	{
		return false;
	}

	WriteTemporaryFile(TEXT("reimport.ls"), SecondLegacyText);
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestTrue(TEXT("The asset holds .lsys text"), Cambriana::DetectGrammarFormat(Grammar->GrammarText) == ECambrianaGrammarFormat::Cambriana);

	const FCoreResult First(ECambrianaGrammarFormat::Legacy, FirstLegacyText, 12345);
	const FCoreResult Second(ECambrianaGrammarFormat::Legacy, SecondLegacyText, 12345);
	const FCoreResult Converted(ECambrianaGrammarFormat::Cambriana, Grammar->GrammarText, 12345);
	if (!TestTrue(TEXT("First file built"), First.IsValid()) || !TestTrue(TEXT("Changed file built"), Second.IsValid())
		|| !TestTrue(TEXT("Asset's grammar built"), Converted.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("Changed file has no errors"), Second.HasErrors());
	TestFalse(TEXT("Asset's grammar has no errors"), Converted.HasErrors());
	TestNotEqual(TEXT("The two files build different bone trees"), Second.Bones().Num(), First.Bones().Num());
	if (!TestEqual(TEXT("Bone count matches the changed file"), Converted.Bones().Num(), Second.Bones().Num()))
	{
		return false;
	}
	TestEqual(TEXT("Bones that differ from the changed file's"), CountBoneMismatches(Converted.Bones(), Second.Bones()), 0);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportRejectedLegacyTest,
	"Cambriana.Editor.Reimport.RejectedLegacyFileReportsLineAndColumnAndLeavesTheAssetUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportRejectedLegacyTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport-rejected.ls"), FirstLegacyText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .ls file creates a grammar asset"), Grammar))
	{
		return false;
	}
	const FString TextBefore = Grammar->GrammarText;
	const uint32 SeedBefore = Grammar->Seed;

	WriteTemporaryFile(TEXT("reimport-rejected.ls"), TEXT("3\n25\n50\nA\nA=F@F@\n@\n"));
	AddExpectedMessagePlain(TEXT("reimport-rejected.ls: error at line 5, column 4: not convertible"), ELogVerbosity::Error);
	AddExpectedMessagePlain(TEXT("reimport-rejected.ls: error at line 5, column 6: not convertible"), ELogVerbosity::Error);
	TestFalse(TEXT("The reimport fails"), Reimport(Grammar));
	TestEqualSensitive(TEXT("The asset's text is unchanged"), Grammar->GrammarText, TextBefore);
	TestEqual(TEXT("The asset's seed is unchanged"), Grammar->Seed, SeedBefore);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportKeepsSeedTest,
	"Cambriana.Editor.Reimport.FileWithoutSeedKeepsTheAssetSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportKeepsSeedTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* FileName;
		const TCHAR* FirstText;
		const TCHAR* SecondText;
	};
	for (const FCase& Case : { FCase{ TEXT("reimport-seed.lsys"), FirstLsysText, SecondLsysText },
			 FCase{ TEXT("reimport-seed.ls"), FirstLegacyText, SecondLegacyText } })
	{
		const FString Path = WriteTemporaryFile(Case.FileName, Case.FirstText);
		const FImportedGrammar Imported(Path);
		UCambrianaGrammar* Grammar = Imported.Grammar();
		if (!TestNotNull(FString::Printf(TEXT("Importing %s creates a grammar asset"), Case.FileName), Grammar))
		{
			continue;
		}
		Grammar->Seed = 31337;
		WriteTemporaryFile(Case.FileName, Case.SecondText);
		TestTrue(FString::Printf(TEXT("Reimporting %s succeeds"), Case.FileName), Reimport(Grammar));
		TestEqual(FString::Printf(TEXT("Reimporting %s keeps the asset's seed"), Case.FileName), Grammar->Seed, 31337u);
		IFileManager::Get().Delete(*Path);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportLsysSeedTest,
	"Cambriana.Editor.Reimport.LsysSeedSettingBecomesTheAssetSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportLsysSeedTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport-seeded.lsys"), FirstLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	Grammar->Seed = 31337;
	WriteTemporaryFile(TEXT("reimport-seeded.lsys"), TEXT("generations 3\nseed 99\naxiom A\n\ngrow {\n    A -> F A\n}\n"));
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestEqual(TEXT("The asset's seed is the file's seed"), Grammar->Seed, 99u);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportRebuildsLiveActorTest,
	"Cambriana.Editor.Reimport.ReferencingLiveActorRebuilds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportRebuildsLiveActorTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport-actor.lsys"), FirstLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Grammar);
	if (!TestNotNull(TEXT("A live actor was placed"), Actor))
	{
		return false;
	}
	TestEqualSensitive(TEXT("The actor was built from the first text"), Actor->GetLastBuildInput().GrammarText, FString(FirstLsysText));
	const int32 BuildsBefore = Actor->GetBuildCount();

	WriteTemporaryFile(TEXT("reimport-actor.lsys"), SecondLsysText);
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestEqual(TEXT("The actor rebuilt once"), Actor->GetBuildCount(), BuildsBefore + 1);
	TestEqualSensitive(TEXT("The actor was built from the changed text"), Actor->GetLastBuildInput().GrammarText, FString(SecondLsysText));
	Actor->GetWorld()->EditorDestroyActor(Actor, false);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportActionTest,
	"Cambriana.Editor.Reimport.ContentBrowserReimportActionIsAvailableAndReimportsTheSourceFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportActionTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport-action.lsys"), FirstLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}

	const TSharedPtr<IAssetTypeActions> Actions = FAssetToolsModule::GetModule().Get().GetAssetTypeActionsForClass(UCambrianaGrammar::StaticClass()).Pin();
	TestTrue(TEXT("Asset tools treat grammar assets as imported, so the Content Browser shows its imported-asset actions"),
		Actions.IsValid() && Actions->IsImportedAsset());

	const FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(FSoftObjectPath(Grammar));
	const UAssetDefinition* Definition = UAssetDefinitionRegistry::Get()->GetAssetDefinitionForAsset(AssetData);
	if (!TestTrue(TEXT("The asset registry has the asset"), AssetData.IsValid()) || !TestNotNull(TEXT("The asset has an asset definition"), Definition))
	{
		return false;
	}
	TArray<FString> SourceFiles;
	FAssetSourceFilesArgs Args;
	Args.Assets = MakeArrayView(&AssetData, 1);
	Args.FilePathFormat = EPathUse::AbsolutePath;
	Definition->GetSourceFiles(Args, [&SourceFiles](const FAssetSourceFilesResult& Result)
	{
		SourceFiles.Add(Result.FilePath);
		return true;
	});
	if (TestEqual(TEXT("The Reimport action has one source file"), SourceFiles.Num(), 1))
	{
		TestTrue(TEXT("The Reimport action's source file is the imported file"), FPaths::IsSamePath(SourceFiles[0], Path));
	}

	WriteTemporaryFile(TEXT("reimport-action.lsys"), SecondLsysText);
	TArray<UObject*> Selected = { Grammar };
	FReimportManager::Instance()->ValidateAllSourceFileAndReimport(Selected, false, INDEX_NONE, false, true);
	TestEqualSensitive(TEXT("The Reimport action reads the changed source file"), Grammar->GrammarText, FString(SecondLsysText));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaSaveLeavesSourceFileTest,
	"Cambriana.Editor.Import.SavingAChangedAssetLeavesItsSourceFileUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaSaveLeavesSourceFileTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Fixture : { TEXT("sk.ls"), TEXT("sk.lsys") })
	{
		TArray<uint8> FixtureBytes;
		if (!TestTrue(FString::Printf(TEXT("%s fixture read"), Fixture), FFileHelper::LoadFileToArray(FixtureBytes, *FixturePath(Fixture))))
		{
			continue;
		}
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaImport"), FString(TEXT("save-")) + Fixture));
		FFileHelper::SaveArrayToFile(FixtureBytes, *Path);

		const FImportedGrammar Imported(Path);
		UCambrianaGrammar* Grammar = Imported.Grammar();
		if (!TestNotNull(FString::Printf(TEXT("Importing %s creates a grammar asset"), Fixture), Grammar))
		{
			continue;
		}
		Grammar->Modify();
		Grammar->GrammarText += TEXT("\n# changed in the editor\n");
		Grammar->Seed += 1;
		Grammar->MarkPackageDirty();
		UPackage* Package = Grammar->GetOutermost();
		TestTrue(FString::Printf(TEXT("The asset imported from %s saves"), Fixture), UEditorLoadingAndSavingUtils::SavePackages({ Package }, false));
		const FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		TestTrue(FString::Printf(TEXT("The asset imported from %s is on disk"), Fixture), IFileManager::Get().FileExists(*PackageFile));

		TArray<uint8> SourceBytes;
		FFileHelper::LoadFileToArray(SourceBytes, *Path);
		TestTrue(FString::Printf(TEXT("Saving the asset leaves the bytes of %s unchanged"), Fixture), SourceBytes == FixtureBytes);
		IFileManager::Get().Delete(*Path);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportRebuildsGrammarEditorPreviewTest,
	"Cambriana.Editor.Reimport.OpenGrammarEditorPreviewRebuildsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportRebuildsGrammarEditorPreviewTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport-preview.lsys"), FirstLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	AssetEditors->OpenEditorForAsset(Grammar);
	IAssetEditorInstance* Editor = AssetEditors->FindEditorForAsset(Grammar, false);
	if (!TestTrue(TEXT("The grammar opens in the grammar editor"), Editor && Editor->GetEditorName() == FCambrianaGrammarEditorToolkit::ToolkitName))
	{
		AssetEditors->CloseAllEditorsForAsset(Grammar);
		return false;
	}
	const FCambrianaGrammarPreview& Preview = static_cast<FCambrianaGrammarEditorToolkit*>(Editor)->GetPreview();
	const UDynamicMeshComponent* Mesh = Preview.GetMeshComponent();
	if (TestNotNull(TEXT("The preview has a mesh"), Mesh))
	{
		TestEqual(TEXT("The first text's 3 segments in one chain at 8 sides: 4 rings of 8"), Mesh->GetMesh()->VertexCount(), 4 * 8);
	}
	const int32 BuildsBefore = Preview.GetBuildCount();

	WriteTemporaryFile(TEXT("reimport-preview.lsys"), SecondLsysText);
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestEqual(TEXT("The reimport rebuilt the preview once"), Preview.GetBuildCount(), BuildsBefore + 1);
	TestEqualSensitive(TEXT("The preview built the reimported text"), Preview.GetLastBuildInput().GrammarText, FString(SecondLsysText));
	AssetEditors->CloseAllEditorsForAsset(Grammar);
	IFileManager::Get().Delete(*Path);
	return true;
}

namespace CambrianaImportSettingsTests
{
	const TCHAR* StatingLsysText = TEXT("generations 3\nsize 50cm\nsides 3\nsurface blocks\nminthickness 5cm\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	const TCHAR* RestatingLsysText = TEXT("generations 3\nstep 2cm\nsides 4\nsurface tubes\nminthickness 1cm\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	const TCHAR* Defaults = TEXT("size Fit 200, sides 8, surface tubes, min thickness 0");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportSettingsFromTextTest,
	"Cambriana.Editor.Import.SizeAndSurfacerSettingsStartFromTheTextOtherwiseTheDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportSettingsFromTextTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaImportSettingsTests;
	struct FCase
	{
		const TCHAR* FileName;
		const TCHAR* Text;
		const TCHAR* Expected;
	};
	const FCase Cases[] = {
		{ TEXT("stating.lsys"), StatingLsysText, TEXT("size Fit 50, sides 3, surface blocks, min thickness 5") },
		{ TEXT("restating.lsys"), RestatingLsysText, TEXT("size Step 2, sides 4, surface tubes, min thickness 1") },
		{ TEXT("partly-stating.lsys"), TEXT("generations 3\nsides 5\naxiom A\n\ngrow {\n    A -> F A\n}\n"),
			TEXT("size Fit 200, sides 5, surface tubes, min thickness 0") },
		{ TEXT("converted.ls"), FirstLegacyText, Defaults },
	};
	for (const FCase& Case : Cases)
	{
		const FString Path = WriteTemporaryFile(Case.FileName, Case.Text);
		const FImportedGrammar Imported(Path);
		const UCambrianaGrammar* Grammar = Imported.Grammar();
		if (TestNotNull(FString::Printf(TEXT("Importing %s creates a grammar asset"), Case.FileName), Grammar))
		{
			TestEqual(FString::Printf(TEXT("The settings of the asset imported from %s"), Case.FileName), DescribeSizeAndSurfacer(*Grammar),
				FString(Case.Expected));
		}
		IFileManager::Get().Delete(*Path);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportKeepsSettingsTest,
	"Cambriana.Editor.Reimport.AssetSizeAndSurfacerSettingsAreUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportKeepsSettingsTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaImportSettingsTests;
	const FString Path = WriteTemporaryFile(TEXT("reimport-settings.lsys"), StatingLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	const FString ImportedSettings = DescribeSizeAndSurfacer(*Grammar);
	TestEqual(TEXT("The imported asset's settings are the text's"), ImportedSettings, FString(TEXT("size Fit 50, sides 3, surface blocks, min thickness 5")));
	WriteTemporaryFile(TEXT("reimport-settings.lsys"), RestatingLsysText);
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestEqualSensitive(TEXT("The reimport replaced the text"), Grammar->GrammarText, FString(RestatingLsysText));
	TestEqual(TEXT("The reimport leaves the asset's settings"), DescribeSizeAndSurfacer(*Grammar), ImportedSettings);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaTextEditKeepsSettingsTest,
	"Cambriana.Editor.GrammarTextEditLeavesTheAssetSizeAndSurfacerSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaTextEditKeepsSettingsTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaImportSettingsTests;
	const FString Path = WriteTemporaryFile(TEXT("edit-settings.lsys"), StatingLsysText);
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	SetGrammarText(Grammar, RestatingLsysText);
	TestEqual(TEXT("Editing the text leaves the asset's settings"), DescribeSizeAndSurfacer(*Grammar),
		FString(TEXT("size Fit 50, sides 3, surface blocks, min thickness 5")));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaImportUppercaseExtensionTest,
	"Cambriana.Editor.Import.ALegacyFileWithAnUppercaseExtensionIsConvertedOnImportAndReimport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaImportUppercaseExtensionTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("upper-case.LS"), FirstLegacyText);
	const FTempFiles Cleanup({ Path });
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing a .LS file creates a grammar asset"), Grammar))
	{
		return false;
	}
	TestTrue(TEXT("The asset holds .lsys text"), Cambriana::DetectGrammarFormat(Grammar->GrammarText) == ECambrianaGrammarFormat::Cambriana);
	const FString FirstText = Grammar->GrammarText;
	// FMT-grammar.md, Conversion: the header numbers become settings and the rule a `grow` rule; whitespace between tokens is not compared.
	TArray<FString> Tokens;
	FirstText.ParseIntoArrayWS(Tokens);
	TestEqual(TEXT("The converted text is the legacy file's header and rule written as .lsys"), FString::Join(Tokens, TEXT(" ")),
		FString(TEXT("generations 3 angle 25 thickness 50% axiom A grow { A -> + F ' A }")));

	WriteTemporaryFile(TEXT("upper-case.LS"), SecondLegacyText);
	TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	TestTrue(TEXT("The reimported asset holds .lsys text"), Cambriana::DetectGrammarFormat(Grammar->GrammarText) == ECambrianaGrammarFormat::Cambriana);
	TestNotEqual(TEXT("The reimport replaced the text"), Grammar->GrammarText, FirstText);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportEffectsTest,
	"Cambriana.Editor.Reimport.ReimportRecordsTheFileMarksThePackageDirtyAndCanBeUndone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportEffectsTest::RunTest(const FString& Parameters)
{
	const FString Path = WriteTemporaryFile(TEXT("reimport-effects.lsys"), FirstLsysText);
	const FTempFiles Cleanup({ Path });
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	if (!TestEqual(TEXT("The import recorded one source file"), Grammar->AssetImportData->GetSourceData().SourceFiles.Num(), 1))
	{
		return false;
	}
	const FMD5Hash HashBefore = Grammar->AssetImportData->GetSourceData().SourceFiles[0].FileHash;
	TestTrue(TEXT("The import recorded the first file's hash"), HashBefore == FMD5Hash::HashFile(*Path));
	Grammar->GetPackage()->SetDirtyFlag(false);

	WriteTemporaryFile(TEXT("reimport-effects.lsys"), SecondLsysText);
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Reimport")));
		TestTrue(TEXT("The reimport succeeds"), Reimport(Grammar));
	}
	if (!TestEqual(TEXT("The reimport kept one source file"), Grammar->AssetImportData->GetSourceData().SourceFiles.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The reimport recorded the changed file's hash"), Grammar->AssetImportData->GetSourceData().SourceFiles[0].FileHash == FMD5Hash::HashFile(*Path));
	TestTrue(TEXT("The hash changed"), Grammar->AssetImportData->GetSourceData().SourceFiles[0].FileHash != HashBefore);
	TestTrue(TEXT("The reimport marked the package dirty"), Grammar->GetPackage()->IsDirty());
	TestTrue(TEXT("The reimport can be undone"), GEditor->UndoTransaction());
	TestEqualSensitive(TEXT("Undo restores the text"), Grammar->GrammarText, FString(FirstLsysText));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportPathsTest,
	"Cambriana.Editor.Reimport.AnAssetWithoutASourceFileCannotBeReimportedAndAReimportPathReplacesTheSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportPathsTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Made = CreateGrammarWithFactory();
	TestFalse(TEXT("A new grammar has no source file to reimport"), FReimportManager::Instance()->CanReimport(Made));

	const FString Path = WriteTemporaryFile(TEXT("reimport-paths.lsys"), FirstLsysText);
	const FString NewPath = WriteTemporaryFile(TEXT("reimport-paths-moved.lsys"), FirstLsysText);
	const FString OtherPath = WriteTemporaryFile(TEXT("reimport-paths-other.lsys"), FirstLsysText);
	const FTempFiles Cleanup({ Path, NewPath, OtherPath });
	const FImportedGrammar Imported(Path);
	UCambrianaGrammar* Grammar = Imported.Grammar();
	if (!TestNotNull(TEXT("Importing the .lsys file creates a grammar asset"), Grammar))
	{
		return false;
	}
	TestTrue(TEXT("An imported grammar can be reimported"), FReimportManager::Instance()->CanReimport(Grammar));
	FReimportManager::Instance()->UpdateReimportPaths(Grammar, { NewPath });
	TestTrue(TEXT("A new reimport path becomes the source file"), FPaths::IsSamePath(Grammar->AssetImportData->GetFirstFilename(), NewPath));
	FReimportManager::Instance()->UpdateReimportPaths(Grammar, { Path, OtherPath });
	TestTrue(TEXT("Two paths for a one-file asset change nothing"), FPaths::IsSamePath(Grammar->AssetImportData->GetFirstFilename(), NewPath));
	return true;
}

#endif
