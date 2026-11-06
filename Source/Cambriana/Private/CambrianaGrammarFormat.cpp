#include "CambrianaGrammarFormat.h"

namespace
{
	bool IsLineBreak(TCHAR Char)
	{
		return Char == TEXT('\n') || Char == TEXT('\r');
	}

	bool IsBlank(TCHAR Char)
	{
		return Char == TEXT(' ') || Char == TEXT('\t') || Char == TEXT('\f') || Char == TEXT('\v');
	}

	bool StartsWithNumber(FStringView Line)
	{
		int32 Index = 0;
		if (Index < Line.Len() && (Line[Index] == TEXT('+') || Line[Index] == TEXT('-')))
		{
			++Index;
		}
		if (Index < Line.Len() && Line[Index] == TEXT('.'))
		{
			++Index;
		}
		return Index < Line.Len() && FChar::IsDigit(Line[Index]);
	}
}

namespace Cambriana
{
	ECambrianaGrammarFormat DetectGrammarFormat(FStringView Text)
	{
		int32 Index = 0;
		while (Index < Text.Len())
		{
			while (Index < Text.Len() && (IsBlank(Text[Index]) || IsLineBreak(Text[Index])))
			{
				++Index;
			}
			if (Index == Text.Len())
			{
				break;
			}
			if (Text[Index] != TEXT('#'))
			{
				return StartsWithNumber(Text.RightChop(Index)) ? ECambrianaGrammarFormat::Legacy : ECambrianaGrammarFormat::Cambriana;
			}
			while (Index < Text.Len() && !IsLineBreak(Text[Index]))
			{
				++Index;
			}
		}
		return ECambrianaGrammarFormat::Cambriana;
	}
}
