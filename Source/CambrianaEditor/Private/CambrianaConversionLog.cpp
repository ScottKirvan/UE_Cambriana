#include "CambrianaConversionLog.h"

#include "CambrianaGrammarConversion.h"
#include "Misc/FeedbackContext.h"

namespace Cambriana::Editor
{
	TOptional<FString> ReportLegacyConversion(const FCambrianaLegacyConversion& Conversion, const FString& Source, const TCHAR* What, FFeedbackContext& Warn)
	{
		if (Conversion.bCoreUnavailable)
		{
			Warn.Logf(ELogVerbosity::Error, TEXT("%s: CambrianaCore is unavailable, so the %s cannot be converted"), *Source, What);
			return {};
		}
		for (const FCambrianaDiagnostic& Diagnostic : Conversion.Diagnostics)
		{
			const ELogVerbosity::Type Verbosity = Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error ? ELogVerbosity::Error : ELogVerbosity::Warning;
			Warn.Logf(Verbosity, TEXT("%s: %s"), *Source, *Diagnostic.Describe());
		}
		if (!Conversion.GrammarText && Conversion.Diagnostics.IsEmpty())
		{
			Warn.Logf(ELogVerbosity::Error, TEXT("%s: the %s could not be converted"), *Source, What);
		}
		return Conversion.GrammarText;
	}
}
