#pragma once

#include "CoreMinimal.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"

class ACambrianaLiveActor;
class UCambrianaGrammar;
class USkeletalMesh;
class USkeleton;
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
	/** The save-asset dialog for the static mesh's location was cancelled. */
	LocationNotChosen,
	/** The static mesh was built but its package did not save. */
	SaveFailed,
};

struct FCambrianaBakeResult
{
	ECambrianaBakeStatus Status = ECambrianaBakeStatus::NothingToBuild;
	/** The static mesh written, or the one that did not save. */
	UStaticMesh* StaticMesh = nullptr;
	/** The skeletal mesh and skeleton written, or those that did not save. */
	USkeletalMesh* SkeletalMesh = nullptr;
	USkeleton* Skeleton = nullptr;
};

namespace Cambriana::Editor
{
	/** Asks whether to overwrite an existing asset; given the question, answers Yes or No. */
	using FAskToOverwrite = TFunction<EAppReturnType::Type(const FText& Question)>;

	/** Asks where to write a live actor's static mesh, given the default package name; answers the chosen package name, or nothing to cancel. */
	using FBakeLocationPrompt = TFunction<FString(const FString& DefaultPackageName)>;

	/** The save-asset dialog's settings for a live actor's static mesh, defaulting to DefaultPackageName. */
	FSaveAssetDialogConfig MakeBakeLocationDialogConfig(const FString& DefaultPackageName);

	/** Answers the live actor Bake's location dialog instead of showing it, for tests; unset, the dialog shows. Returns the previous prompt. */
	FBakeLocationPrompt SetBakeLocationPrompt(FBakeLocationPrompt Prompt);

	inline const FName BakeMenuSection(TEXT("GetAssetActions"));
	inline const FName BakeMenuEntry(TEXT("CambrianaGrammar_Bake"));

	/**
	 * Bakes the grammar asset's saved state into its selected outputs, written to a folder beside the asset named after it, and saves
	 * them. The static mesh is SM_<grammar asset name>. A grammar without saved state is first offered
	 * to the engine's save prompt. AskToOverwrite is asked before an existing static mesh is rebuilt; unset, a message dialog asks.
	 */
	FCambrianaBakeResult BakeGrammar(UCambrianaGrammar& Grammar, const FAskToOverwrite& AskToOverwrite = nullptr);

	/**
	 * Bakes a static mesh from the live actor's own build input, overrides included, and saves it where the engine's save-asset
	 * dialog says, which asks itself before an existing static mesh is replaced. The dialog defaults to SM_<grammar asset
	 * name>_<actor label> in the grammar's bake folder, first offering a grammar without saved state to the engine's save prompt,
	 * or for an actor without a grammar asset to SM_<actor label> at the project's content root.
	 */
	FCambrianaBakeResult BakeLiveActor(ACambrianaLiveActor& Actor);

	/** Shows a notification of a bake's result; Source names what was baked. A declined overwrite or cancelled location shows nothing. */
	void NotifyBakeResult(const FString& Source, const FCambrianaBakeResult& Result);

	/** Adds Bake to the grammar asset's right-click menu in the Content Browser, owned by the current tool menu owner. */
	void AddBakeMenuEntry();
}
