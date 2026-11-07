#include "CambrianaGrammarExporter.h"

#include "CambrianaGrammar.h"
#include "CambrianaGrammarConversion.h"
#include "CambrianaGrammarFormat.h"
#include "Misc/FeedbackContext.h"

namespace
{
	TOptional<FString> ExportedText(const UCambrianaGrammar& Grammar, FFeedbackContext* Warn)
	{
		if (Cambriana::DetectGrammarFormat(Grammar.GrammarText) != ECambrianaGrammarFormat::Legacy)
		{
			return Grammar.GrammarText;
		}

		const FCambrianaLegacyConversion Conversion = Cambriana::ConvertLegacyGrammar(Grammar.GrammarText);
		const FString Name = Grammar.GetPathName();
		if (Conversion.bCoreUnavailable)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("%s: CambrianaCore is unavailable, so the legacy text cannot be converted"), *Name);
			return {};
		}
		for (const FCambrianaDiagnostic& Diagnostic : Conversion.Diagnostics)
		{
			const ELogVerbosity::Type Verbosity = Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error ? ELogVerbosity::Error : ELogVerbosity::Warning;
			Warn->Logf(Verbosity, TEXT("%s: %s"), *Name, *Diagnostic.Describe());
		}
		if (!Conversion.GrammarText && Conversion.Diagnostics.IsEmpty())
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("%s: the legacy text could not be converted"), *Name);
		}
		return Conversion.GrammarText;
	}
}

UCambrianaGrammarExporter::UCambrianaGrammarExporter()
{
	SupportedClass = UCambrianaGrammar::StaticClass();
	bText = false;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("lsys"));
	FormatDescription.Add(TEXT("Cambriana grammar"));
}

bool UCambrianaGrammarExporter::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	const UCambrianaGrammar* Grammar = Cast<UCambrianaGrammar>(Object);
	if (!Grammar)
	{
		return false;
	}
	const TOptional<FString> Text = ExportedText(*Grammar, Warn);
	if (!Text)
	{
		return false;
	}
	FTCHARToUTF8 Utf8(**Text, Text->Len());
	Ar.Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
	return true;
}
