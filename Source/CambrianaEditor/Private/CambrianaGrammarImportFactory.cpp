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
	struct FImportedText
	{
		FString GrammarText;
		uint32 Seed = 0;
	};

	TOptional<FImportedText> ConvertLegacyFile(const FString& Filename, const FString& FileText, FFeedbackContext* Warn)
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
		return FImportedText{ *Conversion.GrammarText, Cambriana::GenerateSeed() };
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
	const FString Extension = FPaths::GetExtension(Filename);
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, *Extension);

	FString FileText;
	TOptional<FImportedText> Imported;
	if (!FFileHelper::LoadFileToString(FileText, *Filename))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("%s: the file could not be read"), *Filename);
	}
	else if (Extension.Equals(TEXT("ls"), ESearchCase::IgnoreCase))
	{
		Imported = ConvertLegacyFile(Filename, FileText, Warn);
	}
	else
	{
		Imported = FImportedText{ FileText, Cambriana::GrammarSeed(FileText).Get(Cambriana::GenerateSeed()) };
	}

	UCambrianaGrammar* Grammar = nullptr;
	if (Imported)
	{
		Grammar = NewObject<UCambrianaGrammar>(InParent, InClass, InName, Flags);
		Grammar->GrammarText = MoveTemp(Imported->GrammarText);
		Grammar->Seed = Imported->Seed;
		Grammar->AssetImportData->Update(Filename);
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Grammar);
	return Grammar;
}
