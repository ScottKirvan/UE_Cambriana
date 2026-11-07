#pragma once

#include "CambrianaBuild.h"
#include "CoreMinimal.h"

/** A legacy .ls grammar converted to the Cambriana grammar format. */
struct FCambrianaLegacyConversion
{
	/** Unset when the core rejects the file or is unavailable. */
	TOptional<FString> GrammarText;
	TArray<FCambrianaDiagnostic> Diagnostics;
	bool bCoreUnavailable = false;
};

namespace Cambriana
{
	/** Converts legacy .ls text to .lsys text; any error diagnostic rejects the file. */
	CAMBRIANA_API FCambrianaLegacyConversion ConvertLegacyGrammar(FStringView LegacyText);

	/** The grammar's seed setting, when the .lsys text states a valid one. */
	CAMBRIANA_API TOptional<uint32> GrammarSeed(FStringView GrammarText);
}
