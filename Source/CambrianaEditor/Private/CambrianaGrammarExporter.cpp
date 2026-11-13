#include "CambrianaGrammarExporter.h"

#include "CambrianaConversionLog.h"
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

		return Cambriana::Editor::ReportLegacyConversion(Cambriana::ConvertLegacyGrammar(Grammar.GrammarText), Grammar.GetPathName(), TEXT("legacy text"), *Warn);
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
