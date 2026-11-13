#pragma once

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"

class ACambrianaLiveActor;
class UCambrianaGrammar;
class UStaticMesh;

enum class ECambrianaBakeStatus : uint8
{
	Baked,
	/** No output the bake writes yet is selected in the grammar's bake settings. */
	NoOutputSelected,
	/** The grammar has no saved state to bake from: its save was declined or did not complete. */
	GrammarNotSaved,
	/** The grammar has errors or builds no triangles. */
	NothingToBuild,
	/** An asset that is not a static mesh holds the static mesh's name. */
	NameTaken,
	OverwriteDeclined,
	/** The static mesh was built but its package did not save. */
	SaveFailed,
};

struct FCambrianaBakeResult
{
	ECambrianaBakeStatus Status = ECambrianaBakeStatus::NothingToBuild;
	/** The static mesh written, or the one that did not save. */
	UStaticMesh* StaticMesh = nullptr;
};

namespace Cambriana::Editor
{
	/** Asks whether to overwrite an existing asset; given the question, answers Yes or No. */
	using FAskToOverwrite = TFunction<EAppReturnType::Type(const FText& Question)>;

	inline const FName BakeMenuSection(TEXT("GetAssetActions"));
	inline const FName BakeMenuEntry(TEXT("CambrianaGrammar_Bake"));

	/**
	 * Bakes the grammar asset's saved state into its selected outputs, written to a folder beside the asset named after it, and saves
	 * them. The static mesh is SM_<grammar asset name>. A grammar without saved state is first offered
	 * to the engine's save prompt. AskToOverwrite is asked before an existing static mesh is rebuilt; unset, a message dialog asks.
	 */
	FCambrianaBakeResult BakeGrammar(UCambrianaGrammar& Grammar, const FAskToOverwrite& AskToOverwrite = nullptr);

	/**
	 * Bakes a static mesh from the live actor's own build input, overrides included, and saves it. An actor with a grammar asset
	 * writes SM_<grammar asset name>_<actor label> into the grammar's bake folder, first offering a grammar without saved state to
	 * the engine's save prompt; one without writes SM_<actor label> at the project's content root. AskToOverwrite is asked
	 * before an existing static mesh is rebuilt; unset, a message dialog asks.
	 */
	FCambrianaBakeResult BakeLiveActor(ACambrianaLiveActor& Actor, const FAskToOverwrite& AskToOverwrite = nullptr);

	/** Adds Bake to the grammar asset's right-click menu in the Content Browser, owned by the current tool menu owner. */
	void AddBakeMenuEntry();
}
