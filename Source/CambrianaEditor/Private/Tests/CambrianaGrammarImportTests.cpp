#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarFormat.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"

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

	FString LoadFile(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
	}

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
	TestEqual(TEXT("The asset holds the file's text"), Grammar->GrammarText, FileText);
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

#endif
