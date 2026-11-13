#pragma once

#include "CambrianaBuild.h"
#include "CoreMinimal.h"

struct cambriana_grammar;

namespace Cambriana
{
	/** Owns a core grammar model: Cambriana grammar text parsed, or legacy text converted. Empty until one of those succeeds. */
	class FCoreGrammar
	{
	public:
		FCoreGrammar() = default;
		~FCoreGrammar();
		FCoreGrammar(const FCoreGrammar&) = delete;
		FCoreGrammar& operator=(const FCoreGrammar&) = delete;

		/** Parses Cambriana grammar text; returns whether the core made a model. */
		bool Parse(const FTCHARToUTF8& Text);
		/** Converts legacy .ls text; returns whether the core made a model. */
		bool ConvertLegacy(const FTCHARToUTF8& Text);

		cambriana_grammar* Get() const { return Grammar; }

		/** The value the grammar states for a CAMBRIANA_SETTING_* setting; unset when it states none or there is no model. */
		TOptional<double> GetSetting(int32 Setting) const;
		FCambrianaStatedSettings GetStatedSettings() const;

		/** Sets each declared parameter to its stored value; the core ignores undeclared names and an empty model. */
		void ApplyParameterValues(TConstArrayView<FCambrianaParameterValue> Values);
		/** The declared parameters in file order, each with the model's value in Value. */
		TArray<FCambrianaParameter> ReadParameters() const;

	private:
		void Reset();

		cambriana_grammar* Grammar = nullptr;
	};
}
