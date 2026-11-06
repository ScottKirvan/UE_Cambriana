#pragma once

#include "CoreMinimal.h"

enum class ECambrianaGrammarFormat : uint8
{
	Legacy,
	Cambriana,
};

namespace Cambriana
{
	/** Legacy when the first line that is neither blank nor a # comment starts with a number; otherwise Cambriana. */
	CAMBRIANA_API ECambrianaGrammarFormat DetectGrammarFormat(FStringView Text);
}
