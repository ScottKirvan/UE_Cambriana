#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarFormat.h"
#include "EditorFramework/AssetImportData.h"
#include "Exporters/Exporter.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	FString ExportTestPath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("CambrianaExport"), FileName));
	}

	FString LoadFile(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
	}

	TOptional<FString> LoadUtf8File(const FString& Path)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			return {};
		}
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
	}

	UCambrianaGrammar* NewGrammar(const FString& GrammarText)
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage(), NAME_None, RF_Transient);
		Grammar->GrammarText = GrammarText;
		Grammar->Seed = 12345;
		return Grammar;
	}

	bool Export(UCambrianaGrammar* Grammar, const FString& Path)
	{
		IFileManager::Get().Delete(*Path);
		return UExporter::ExportToFile(Grammar, nullptr, *Path, false) != 0;
	}

	class FParsedGrammar
	{
	public:
		explicit FParsedGrammar(const FString& Text)
		{
			const FTCHARToUTF8 Utf8(*Text);
			cambriana_grammar_parse(Utf8.Get(), static_cast<size_t>(Utf8.Length()), &Grammar);
		}

		~FParsedGrammar() { cambriana_grammar_destroy(Grammar); }

		FParsedGrammar(const FParsedGrammar&) = delete;
		FParsedGrammar& operator=(const FParsedGrammar&) = delete;

		bool IsValid() const { return Grammar != nullptr; }

		int32 NumErrors() const
		{
			uint64_t Count = 0;
			const cambriana_diagnostic* Diagnostics = cambriana_grammar_diagnostics(Grammar, &Count);
			int32 Errors = 0;
			for (uint64_t Index = 0; Diagnostics && Index < Count; ++Index)
			{
				Errors += Diagnostics[Index].severity == CAMBRIANA_SEVERITY_ERROR;
			}
			return Errors;
		}

		bool Equals(const FParsedGrammar& Other) const { return cambriana_grammar_equal(Grammar, Other.Grammar) == 1; }

	private:
		cambriana_grammar* Grammar = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaExportParsesToEqualGrammarTest,
	"Cambriana.Editor.Export.ExportedFileParsesToAGrammarEqualToTheAssetsAndLeavesTheSourceFileUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaExportParsesToEqualGrammarTest::RunTest(const FString& Parameters)
{
	TArray<uint8> SourceBytes;
	if (!TestTrue(TEXT("sk.lsys fixture read"), FFileHelper::LoadFileToArray(SourceBytes, *FixturePath(TEXT("sk.lsys")))))
	{
		return false;
	}
	const FString SourcePath = ExportTestPath(TEXT("export-source.lsys"));
	FFileHelper::SaveArrayToFile(SourceBytes, *SourcePath);

	UCambrianaGrammar* Grammar = NewGrammar(LoadFile(SourcePath) + TEXT("\n# edited in the editor\n"));
	Grammar->AssetImportData->Update(SourcePath);

	const FString ExportPath = ExportTestPath(TEXT("exported.lsys"));
	TestTrue(TEXT("The export succeeds"), Export(Grammar, ExportPath));
	const TOptional<FString> Exported = LoadUtf8File(ExportPath);
	if (!TestTrue(TEXT("The exported file exists"), Exported.IsSet()))
	{
		return false;
	}

	const FParsedGrammar AssetGrammar(Grammar->GrammarText);
	const FParsedGrammar ExportedGrammar(*Exported);
	if (!TestTrue(TEXT("The asset's text parsed"), AssetGrammar.IsValid()) || !TestTrue(TEXT("The exported text parsed"), ExportedGrammar.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("Errors in the asset's grammar"), AssetGrammar.NumErrors(), 0);
	TestEqual(TEXT("Errors in the exported grammar"), ExportedGrammar.NumErrors(), 0);
	TestTrue(TEXT("The exported grammar equals the asset's"), ExportedGrammar.Equals(AssetGrammar));

	TArray<uint8> SourceBytesAfter;
	FFileHelper::LoadFileToArray(SourceBytesAfter, *SourcePath);
	TestTrue(TEXT("Exporting leaves the asset's source file unchanged"), SourceBytesAfter == SourceBytes);
	IFileManager::Get().Delete(*SourcePath);
	IFileManager::Get().Delete(*ExportPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaExportLegacyAsLsysTest,
	"Cambriana.Editor.Export.LegacyTextIsExportedAsLsysThatBuildsTheSameBones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaExportLegacyAsLsysTest::RunTest(const FString& Parameters)
{
	const FString LegacyText = LoadFile(FixturePath(TEXT("sk.ls")));
	if (!TestFalse(TEXT("The sk.ls fixture has text"), LegacyText.IsEmpty()))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = NewGrammar(LegacyText);
	TestTrue(TEXT("The asset holds legacy text"), Cambriana::DetectGrammarFormat(Grammar->GrammarText) == ECambrianaGrammarFormat::Legacy);

	const FString ExportPath = ExportTestPath(TEXT("legacy.lsys"));
	TestTrue(TEXT("The export succeeds"), Export(Grammar, ExportPath));
	const TOptional<FString> Exported = LoadUtf8File(ExportPath);
	if (!TestTrue(TEXT("The exported file exists"), Exported.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("The exported file holds .lsys text"), Cambriana::DetectGrammarFormat(*Exported) == ECambrianaGrammarFormat::Cambriana);

	const FCoreResult Legacy(ECambrianaGrammarFormat::Legacy, LegacyText, 12345);
	const FCoreResult Converted(ECambrianaGrammarFormat::Cambriana, *Exported, 12345);
	if (!TestTrue(TEXT("Legacy text built"), Legacy.IsValid()) || !TestTrue(TEXT("Exported text built"), Converted.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("Legacy text has no errors"), Legacy.HasErrors());
	TestFalse(TEXT("Exported text has no errors"), Converted.HasErrors());
	TestTrue(TEXT("Legacy text has bones"), Legacy.Bones().Num() > 0);
	if (!TestEqual(TEXT("Bone count"), Converted.Bones().Num(), Legacy.Bones().Num()))
	{
		return false;
	}
	TestEqual(TEXT("Bones that differ from the legacy text's"), CountBoneMismatches(Converted.Bones(), Legacy.Bones()), 0);
	IFileManager::Get().Delete(*ExportPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaExportActionOfferedTest,
	"Cambriana.Editor.Export.ExportActionIsOfferedForAGrammarAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaExportActionOfferedTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = NewGrammar(LoadFile(FixturePath(TEXT("sk.lsys"))));
	const UExporter* Exporter = UExporter::FindExporter(Grammar, TEXT("lsys"));
	if (TestNotNull(TEXT("The engine finds a .lsys exporter for a grammar asset"), Exporter))
	{
		TestTrue(TEXT("The exporter supports grammar assets"), Exporter->SupportsObject(Grammar));
	}
	return true;
}

#endif
