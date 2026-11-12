#pragma once

#include "CoreMinimal.h"

class UCambrianaGrammar;

/** The grammar editor's working copy: a transient duplicate of a grammar asset, edited in place of it until a save. */
namespace Cambriana::Editor
{
	/** A transactional duplicate of Grammar in the transient package, neither standalone nor public. */
	UCambrianaGrammar* DuplicateWorkingCopy(const UCambrianaGrammar& Grammar);

	/** Whether A and B hold equal values for every property the grammar class declares, except its import data. */
	bool HoldSameGrammar(const UCambrianaGrammar& A, const UCambrianaGrammar& B);

	/** Copies those properties from From into To as one change event outside the undo history; returns false, changing nothing, when they already hold the same grammar. */
	bool CopyGrammar(const UCambrianaGrammar& From, UCambrianaGrammar& To);
}
