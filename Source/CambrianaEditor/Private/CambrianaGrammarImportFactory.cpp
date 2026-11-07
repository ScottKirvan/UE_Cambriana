#include "CambrianaGrammarImportFactory.h"

#include "CambrianaGrammar.h"
#include "CambrianaGrammarConversion.h"
#include "CambrianaSeed.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/ImportSubsystem.h"

namespace
{
	struct FSourceGrammar
	{
		FString GrammarText;
		TOptional<uint32> Seed;
	};

	TOptional<FString> ConvertLegacyFile(const FString& Filename, const FString& FileText, FFeedbackContext* Warn)
	{
		const FCambrianaLegacyConversion Conversion = Cambriana::ConvertLegacyGrammar(FileText);
		if (Conversion.bCoreUnavailable)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("%s: CambrianaCore is unavailable, so the file cannot be converted"), *Filename);
			return {};
		}
		for (const FCambrianaDiagnostic& Diagnostic : Conversion.Diagnostics)
		{
			const ELogVerbosity::Type Verbosity = Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error ? ELogVerbosity::Error : ELogVerbosity::Warning;
			Warn->Logf(Verbosity, TEXT("%s: %s"), *Filename, *Diagnostic.Describe());
		}
		if (!Conversion.GrammarText)
		{
			if (Conversion.Diagnostics.IsEmpty())
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("%s: the file could not be converted"), *Filename);
			}
			return {};
		}
		return *Conversion.GrammarText;
	}

	TOptional<FSourceGrammar> ReadSourceGrammar(const FString& Filename, FFeedbackContext* Warn)
	{
		FString FileText;
		if (!FFileHelper::LoadFileToString(FileText, *Filename))
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("%s: the file could not be read"), *Filename);
			return {};
		}
		if (FPaths::GetExtension(Filename).Equals(TEXT("ls"), ESearchCase::IgnoreCase))
		{
			TOptional<FString> Converted = ConvertLegacyFile(Filename, FileText, Warn);
			return Converted ? FSourceGrammar{ MoveTemp(*Converted), {} } : TOptional<FSourceGrammar>();
		}
		const TOptional<uint32> Seed = Cambriana::GrammarSeed(FileText);
		return FSourceGrammar{ MoveTemp(FileText), Seed };
	}

	UCambrianaGrammar* GrammarWithSourceFile(UObject* Obj)
	{
		UCambrianaGrammar* Grammar = Cast<UCambrianaGrammar>(Obj);
		return Grammar && Grammar->AssetImportData && !Grammar->AssetImportData->GetFirstFilename().IsEmpty() ? Grammar : nullptr;
	}
}

UCambrianaGrammarImportFactory::UCambrianaGrammarImportFactory()
{
	SupportedClass = UCambrianaGrammar::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	Formats.Add(TEXT("lsys;Cambriana grammar"));
	Formats.Add(TEXT("ls;Lparser grammar"));
}

UObject* UCambrianaGrammarImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, *FPaths::GetExtension(Filename));

	UCambrianaGrammar* Grammar = nullptr;
	if (TOptional<FSourceGrammar> Source = ReadSourceGrammar(Filename, Warn))
	{
		Grammar = NewObject<UCambrianaGrammar>(InParent, InClass, InName, Flags);
		Grammar->GrammarText = MoveTemp(Source->GrammarText);
		Grammar->Seed = Source->Seed.Get(Cambriana::GenerateSeed());
		Grammar->AssetImportData->Update(Filename);
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Grammar);
	return Grammar;
}

bool UCambrianaGrammarImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const UCambrianaGrammar* Grammar = GrammarWithSourceFile(Obj);
	if (!Grammar)
	{
		return false;
	}
	Grammar->AssetImportData->ExtractFilenames(OutFilenames);
	return true;
}

void UCambrianaGrammarImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UCambrianaGrammar* Grammar = Cast<UCambrianaGrammar>(Obj);
	if (Grammar && Grammar->AssetImportData && NewReimportPaths.Num() == 1)
	{
		Grammar->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UCambrianaGrammarImportFactory::Reimport(UObject* Obj)
{
	UCambrianaGrammar* Grammar = GrammarWithSourceFile(Obj);
	if (!Grammar)
	{
		return EReimportResult::Failed;
	}
	const FString Filename = Grammar->AssetImportData->GetFirstFilename();
	TOptional<FSourceGrammar> Source = ReadSourceGrammar(Filename, GWarn);
	if (!Source)
	{
		return EReimportResult::Failed;
	}

	Grammar->Modify();
	Grammar->GrammarText = MoveTemp(Source->GrammarText);
	Grammar->Seed = Source->Seed.Get(Grammar->Seed);
	Grammar->AssetImportData->Update(Filename);
	Grammar->MarkPackageDirty();
	return EReimportResult::Succeeded;
}
