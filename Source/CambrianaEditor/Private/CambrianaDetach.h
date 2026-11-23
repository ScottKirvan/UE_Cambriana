#pragma once

#include "CoreMinimal.h"

namespace Cambriana::Editor
{
	inline const FName DetachMenuSection(TEXT("GetAssetActions"));
	/** The dynamic entry that offers Detach from Grammar when the selection holds a mesh linked to a grammar. */
	inline const FName DetachFromGrammarDynamicEntry(TEXT("CambrianaMesh_DetachFromGrammarDynamic"));
	inline const FName DetachFromGrammarMenuEntry(TEXT("CambrianaMesh_DetachFromGrammar"));

	/**
	 * Detaches each of the meshes that is linked to a grammar: its import data becomes plain import data with no source file, as the
	 * Source File row's Clear leaves it, its package is marked dirty, and all of them change in one undoable transaction. Returns how
	 * many were detached.
	 */
	int32 DetachFromGrammar(TConstArrayView<UObject*> Meshes);

	/** Adds Detach from Grammar to the static and skeletal mesh right-click menus in the Content Browser, owned by the current tool menu owner. */
	void AddDetachMenuEntry();
}
