#include "CambrianaCoreGrammar.h"

#include <cambriana/cambriana.h>

namespace
{
	ECambrianaParameterTier ToParameterTier(int32 Tier)
	{
		switch (Tier)
		{
		case CAMBRIANA_TIER_POSE: return ECambrianaParameterTier::Pose;
		case CAMBRIANA_TIER_SHAPE: return ECambrianaParameterTier::Shape;
		case CAMBRIANA_TIER_STRUCTURE: return ECambrianaParameterTier::Structure;
		default: return ECambrianaParameterTier::None;
		}
	}
}

namespace Cambriana
{
	FCoreGrammar::~FCoreGrammar()
	{
		Reset();
	}

	void FCoreGrammar::Reset()
	{
		cambriana_grammar_destroy(Grammar);
		Grammar = nullptr;
	}

	bool FCoreGrammar::Parse(const FTCHARToUTF8& Text)
	{
		Reset();
		return cambriana_grammar_parse(Text.Get(), static_cast<size_t>(Text.Length()), &Grammar) == CAMBRIANA_OK && Grammar;
	}

	bool FCoreGrammar::ConvertLegacy(const FTCHARToUTF8& Text)
	{
		Reset();
		return cambriana_grammar_convert_legacy(Text.Get(), static_cast<size_t>(Text.Length()), &Grammar) == CAMBRIANA_OK && Grammar;
	}

	TOptional<double> FCoreGrammar::GetSetting(int32 Setting) const
	{
		double Value = 0.0;
		return Grammar && cambriana_grammar_get_setting(Grammar, Setting, &Value) == CAMBRIANA_OK ? TOptional<double>(Value) : TOptional<double>();
	}

	FCambrianaStatedSettings FCoreGrammar::GetStatedSettings() const
	{
		FCambrianaStatedSettings Stated;
		Stated.Recursion = GetSetting(CAMBRIANA_SETTING_GENERATIONS);
		Stated.BasicAngle = GetSetting(CAMBRIANA_SETTING_ANGLE);
		Stated.Thickness = GetSetting(CAMBRIANA_SETTING_THICKNESS);
		if (const TOptional<double> Size = GetSetting(CAMBRIANA_SETTING_SIZE))
		{
			Stated.SizeMode = ECambrianaSizeMode::Fit;
			Stated.SizeValue = Size;
		}
		else if (const TOptional<double> Step = GetSetting(CAMBRIANA_SETTING_STEP))
		{
			Stated.SizeMode = ECambrianaSizeMode::Step;
			Stated.SizeValue = Step;
		}
		if (const TOptional<double> Sides = GetSetting(CAMBRIANA_SETTING_SIDES))
		{
			Stated.Sides = FMath::RoundToInt32(*Sides);
		}
		if (const TOptional<double> Surface = GetSetting(CAMBRIANA_SETTING_SURFACE))
		{
			Stated.bClosedForm = *Surface != 0.0;
		}
		Stated.MinThickness = GetSetting(CAMBRIANA_SETTING_MIN_THICKNESS);
		return Stated;
	}

	void FCoreGrammar::ApplyParameterValues(TConstArrayView<FCambrianaParameterValue> Values)
	{
		for (const FCambrianaParameterValue& Value : Values)
		{
			cambriana_grammar_set_parameter(Grammar, FTCHARToUTF8(*Value.Name).Get(), Value.Value);
		}
	}

	TArray<FCambrianaParameter> FCoreGrammar::ReadParameters() const
	{
		TArray<FCambrianaParameter> Parameters;
		const uint64 Count = Grammar ? cambriana_grammar_parameter_count(Grammar) : 0;
		for (uint64 Index = 0; Index < Count; ++Index)
		{
			const char* Name = nullptr;
			int32_t Tier = CAMBRIANA_TIER_NONE;
			double Value = 0.0;
			if (cambriana_grammar_parameter(Grammar, Index, &Name, &Tier, &Value) == CAMBRIANA_OK && Name)
			{
				FCambrianaParameter& Parameter = Parameters.AddDefaulted_GetRef();
				Parameter.Name = UTF8_TO_TCHAR(Name);
				Parameter.Tier = ToParameterTier(Tier);
				Parameter.Value = Value;
			}
		}
		return Parameters;
	}
}
