#include "CambrianaGrammar.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TCHAR* SaveTestPackageName = TEXT("/Game/CambrianaSaveTests/SavedGrammar");

	FString SaveTestObjectPath()
	{
		return FString::Printf(TEXT("%s.%s"), SaveTestPackageName, *FPackageName::GetShortName(SaveTestPackageName));
	}

	FString SaveTestPackageFilename()
	{
		return FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(SaveTestPackageName, FPackageName::GetAssetPackageExtension()));
	}

	bool Unload(UPackage* Package)
	{
		FText Error;
		return UPackageTools::UnloadPackages({ Package }, Error, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarSaveRestoreTest,
	"Cambriana.Editor.Save.GrammarTextSeedAndSourcePathSurviveSavingAndReloading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarSaveRestoreTest::RunTest(const FString& Parameters)
{
	const FString PackageFilename = SaveTestPackageFilename();
	IFileManager::Get().Delete(*PackageFilename);

	const FString SourcePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaSave"), TEXT("source.lsys")));
	const FString SourceText = TEXT("generations 3\nseed 7\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	FFileHelper::SaveStringToFile(SourceText, *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UPackage* Package = CreatePackage(SaveTestPackageName);
	UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(Package, *FPackageName::GetShortName(SaveTestPackageName), RF_Public | RF_Standalone);
	Grammar->GrammarText = SourceText;
	Grammar->Seed = 7;
	Grammar->AssetImportData->Update(SourcePath);

	const FString EditedText = SourceText + TEXT("\n# edited in the editor\n");
	const uint32 EditedSeed = 4242;
	Grammar->GrammarText = EditedText;
	Grammar->Seed = EditedSeed;
	TestEqual(TEXT("The asset records its source file before saving"), Grammar->AssetImportData->GetFirstFilename(), SourcePath);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	if (!TestTrue(TEXT("The package saves to disk"), UPackage::SavePackage(Package, Grammar, *PackageFilename, SaveArgs)))
	{
		Unload(Package);
		IFileManager::Get().Delete(*SourcePath);
		return false;
	}
	TestTrue(TEXT("The package file exists"), IFileManager::Get().FileExists(*PackageFilename));

	const TWeakObjectPtr<UCambrianaGrammar> SavedGrammar = Grammar;
	TestTrue(TEXT("The package unloads"), Unload(Package));
	Grammar = nullptr;
	Package = nullptr;
	TestFalse(TEXT("The saved asset is no longer in memory"), SavedGrammar.IsValid());
	TestNull(TEXT("No grammar asset is loaded at the asset's path"), FindObject<UCambrianaGrammar>(nullptr, *SaveTestObjectPath()));

	UPackage* LoadedPackage = LoadPackage(nullptr, SaveTestPackageName, LOAD_None);
	const UCambrianaGrammar* Loaded = LoadedPackage ? FindObject<UCambrianaGrammar>(LoadedPackage, *FPackageName::GetShortName(SaveTestPackageName)) : nullptr;
	if (TestNotNull(TEXT("The grammar asset loads again"), Loaded))
	{
		TestEqual(TEXT("Grammar text"), Loaded->GrammarText, EditedText);
		TestEqual(TEXT("Seed"), Loaded->Seed, EditedSeed);
		if (TestNotNull(TEXT("The loaded asset has import data"), Loaded->AssetImportData.Get()))
		{
			TestEqual(TEXT("Source file path"), Loaded->AssetImportData->GetFirstFilename(), SourcePath);
		}
	}

	if (LoadedPackage)
	{
		Unload(LoadedPackage);
	}
	IFileManager::Get().Delete(*PackageFilename);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename));
	IFileManager::Get().Delete(*SourcePath);
	TestFalse(TEXT("The test package file is deleted"), IFileManager::Get().FileExists(*PackageFilename));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarSettingsSaveRestoreTest,
	"Cambriana.Editor.Save.GrammarSizeAndSurfacerSettingsSurviveSavingAndReloading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarSettingsSaveRestoreTest::RunTest(const FString& Parameters)
{
	const TCHAR* PackageName = TEXT("/Game/CambrianaSaveTests/SavedGrammarSettings");
	const FString ShortName = FPackageName::GetShortName(PackageName);
	const FString PackageFilename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()));
	IFileManager::Get().Delete(*PackageFilename);

	UPackage* Package = CreatePackage(PackageName);
	UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(Package, *ShortName, RF_Public | RF_Standalone);
	Grammar->SizeMode = ECambrianaSizeMode::Step;
	Grammar->SizeValue = 12.5;
	Grammar->bClosedForm = false;
	Grammar->Sides = 3;
	Grammar->MinThickness = 1.5;

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	if (!TestTrue(TEXT("The package saves to disk"), UPackage::SavePackage(Package, Grammar, *PackageFilename, SaveArgs)))
	{
		Unload(Package);
		return false;
	}
	TestTrue(TEXT("The package unloads"), Unload(Package));
	Grammar = nullptr;
	Package = nullptr;
	TestNull(TEXT("No grammar asset is loaded at the asset's path"), FindObject<UCambrianaGrammar>(nullptr, *FString::Printf(TEXT("%s.%s"), PackageName, *ShortName)));

	UPackage* LoadedPackage = LoadPackage(nullptr, PackageName, LOAD_None);
	const UCambrianaGrammar* Loaded = LoadedPackage ? FindObject<UCambrianaGrammar>(LoadedPackage, *ShortName) : nullptr;
	if (TestNotNull(TEXT("The grammar asset loads again"), Loaded))
	{
		TestTrue(TEXT("Size mode"), Loaded->SizeMode == ECambrianaSizeMode::Step);
		TestEqual(TEXT("Size value"), Loaded->SizeValue, 12.5);
		TestFalse(TEXT("Closed form"), Loaded->bClosedForm);
		TestEqual(TEXT("Sides"), Loaded->Sides, 3);
		TestEqual(TEXT("Min thickness"), Loaded->MinThickness, 1.5);
	}

	if (LoadedPackage)
	{
		Unload(LoadedPackage);
	}
	IFileManager::Get().Delete(*PackageFilename);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename));
	TestFalse(TEXT("The test package file is deleted"), IFileManager::Get().FileExists(*PackageFilename));
	return true;
}

#endif
