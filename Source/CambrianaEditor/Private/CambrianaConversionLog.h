#pragma once

#include "CoreMinimal.h"

class FFeedbackContext;
struct FCambrianaLegacyConversion;

namespace Cambriana::Editor
{
	/**
	 * Logs a legacy conversion's diagnostics, and why it failed, against Source ("<Source>: ..."); What names the converted thing in
	 * those messages ("file", "legacy text"). Returns the converted text, unset when the conversion failed.
	 */
	TOptional<FString> ReportLegacyConversion(const FCambrianaLegacyConversion& Conversion, const FString& Source, const TCHAR* What, FFeedbackContext& Warn);
}
