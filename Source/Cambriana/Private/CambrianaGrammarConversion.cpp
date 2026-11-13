#include "CambrianaGrammarConversion.h"

#include "CambrianaBuildInternal.h"
#include "CambrianaCoreGrammar.h"
#include "CambrianaModule.h"

#include <cambriana/cambriana.h>

namespace Cambriana
{
	FCambrianaLegacyConversion ConvertLegacyGrammar(FStringView LegacyText)
	{
		FCambrianaLegacyConversion Conversion;
		if (!FCambrianaModule::IsCoreAvailable())
		{
			Conversion.bCoreUnavailable = true;
			return Conversion;
		}

		const FTCHARToUTF8 Utf8(LegacyText.GetData(), LegacyText.Len());
		FCoreGrammar Core;
		if (!Core.ConvertLegacy(Utf8))
		{
			return Conversion;
		}

		uint64_t Count = 0;
		const cambriana_diagnostic* Diagnostics = cambriana_grammar_diagnostics(Core.Get(), &Count);
		bool bRejected = false;
		for (uint64_t Index = 0; Diagnostics && Index < Count; ++Index)
		{
			bRejected |= Diagnostics[Index].severity == CAMBRIANA_SEVERITY_ERROR;
			Conversion.Diagnostics.Add(ToDiagnostic(Diagnostics[Index]));
		}
		if (bRejected)
		{
			return Conversion;
		}

		size_t Length = 0;
		if (const char* Text = cambriana_grammar_serialise(Core.Get(), CAMBRIANA_STYLE_GLYPHS, &Length))
		{
			const FUTF8ToTCHAR Converted(Text, static_cast<int32>(Length));
			Conversion.GrammarText = FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
		}
		return Conversion;
	}

	TOptional<uint32> GrammarSeed(FStringView GrammarText)
	{
		if (!FCambrianaModule::IsCoreAvailable())
		{
			return {};
		}

		const FTCHARToUTF8 Utf8(GrammarText.GetData(), GrammarText.Len());
		FCoreGrammar Core;
		const TOptional<double> Stated = Core.Parse(Utf8) ? Core.GetSetting(CAMBRIANA_SETTING_SEED) : TOptional<double>();
		if (!Stated)
		{
			return {};
		}
		const double Seed = *Stated;
		if (!FMath::IsFinite(Seed) || Seed < 0.0 || Seed > static_cast<double>(MAX_uint32) || FMath::Frac(Seed) != 0.0)
		{
			return {};
		}
		return static_cast<uint32>(Seed);
	}
}
