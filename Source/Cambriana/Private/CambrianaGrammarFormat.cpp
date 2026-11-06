#include "CambrianaGrammarFormat.h"

namespace Cambriana
{
	ECambrianaGrammarFormat DetectGrammarFormat(FStringView Text)
	{
		return ECambrianaGrammarFormat::Legacy;
	}
}
