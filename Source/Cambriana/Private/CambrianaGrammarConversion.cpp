#include "CambrianaGrammarConversion.h"

#include "CambrianaBuildInternal.h"
#include "CambrianaModule.h"

#include <cambriana/cambriana.h>

namespace
{
	class FCoreGrammar
	{
	public:
		FCoreGrammar() = default;
		~FCoreGrammar() { cambriana_grammar_destroy(Grammar); }
		FCoreGrammar(const FCoreGrammar&) = delete;
		FCoreGrammar& operator=(const FCoreGrammar&) = delete;

		cambriana_grammar* Grammar = nullptr;
	};
}

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
		if (cambriana_grammar_convert_legacy(Utf8.Get(), static_cast<size_t>(Utf8.Length()), &Core.Grammar) != CAMBRIANA_OK || !Core.Grammar)
		{
			return Conversion;
		}

		uint64_t Count = 0;
		const cambriana_diagnostic* Diagnostics = cambriana_grammar_diagnostics(Core.Grammar, &Count);
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
		if (const char* Text = cambriana_grammar_serialise(Core.Grammar, CAMBRIANA_STYLE_GLYPHS, &Length))
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
		double Seed = 0.0;
		if (cambriana_grammar_parse(Utf8.Get(), static_cast<size_t>(Utf8.Length()), &Core.Grammar) != CAMBRIANA_OK || !Core.Grammar
			|| cambriana_grammar_get_setting(Core.Grammar, CAMBRIANA_SETTING_SEED, &Seed) != CAMBRIANA_OK)
		{
			return {};
		}
		if (!FMath::IsFinite(Seed) || Seed < 0.0 || Seed > static_cast<double>(MAX_uint32) || FMath::Frac(Seed) != 0.0)
		{
			return {};
		}
		return static_cast<uint32>(Seed);
	}
}
