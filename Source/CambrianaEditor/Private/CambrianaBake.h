#pragma once

#include "CoreMinimal.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"

class ACambrianaLiveActor;
class UCambrianaGrammar;
class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;
class UStaticMesh;
struct FToolMenuContext;
struct FToolMenuSection;

enum class ECambrianaBakeStatus : uint8
{
	Baked,
	/** The grammar has errors, builds no triangles, or its bone tree makes no skeleton. */
	NothingToBuild,
	/** An asset of another class, or a redirector, holds an output's name and cannot be deleted. */
	NameTaken,
	/** The answer to the overwrite or the replace question was No. */
	OverwriteDeclined,
	/** The save-asset dialog for the static mesh's location was cancelled. */
	LocationNotChosen,
	/** The outputs were built but did not all save. */
	SaveFailed,
	/** Reimport's bones did not merge into the skeleton, and recreating its bone tree was declined. */
	SkeletonRecreateDeclined,
};

/** What a grammar asset's Bake writes: a static mesh, or a skeletal mesh with its skeleton and physics asset. */
enum class ECambrianaBakeChoice : uint8
{
	StaticMesh,
	SkeletalMesh,
};

struct FCambrianaBakeResult
{
	ECambrianaBakeStatus Status = ECambrianaBakeStatus::NothingToBuild;
	/** The static mesh written, or the one that did not save. */
	UStaticMesh* StaticMesh = nullptr;
	/** The skeletal mesh and skeleton written, or those that did not save. */
	USkeletalMesh* SkeletalMesh = nullptr;
	USkeleton* Skeleton = nullptr;
	/** The physics asset written, or the one that did not save. */
	UPhysicsAsset* PhysicsAsset = nullptr;
};

namespace Cambriana::Editor
{
	/** Asks whether to overwrite an existing asset; given the question, answers Yes or No. */
	using FAskToOverwrite = TFunction<EAppReturnType::Type(const FText& Question)>;

	/** Asks whether to replace an existing asset of another class, or a redirector, at an output's name; given the question, answers Yes or No. */
	using FAskToReplace = TFunction<EAppReturnType::Type(const FText& Question)>;

	/** Asks whether to recreate a skeleton's bone tree when the grammar's bones do not merge into it; given the question, answers Yes, No or Cancel. */
	using FAskToRecreateSkeleton = TFunction<EAppReturnType::Type(const FText& Question)>;

	/** Asks where to write a live actor's static or skeletal mesh, given the default package name; answers the chosen package name, or nothing to cancel. */
	using FBakeLocationPrompt = TFunction<FString(const FString& DefaultPackageName)>;

	/** The save-asset dialog's settings for the mesh a live actor's Bake choice writes, that class only, defaulting to DefaultPackageName. */
	FSaveAssetDialogConfig MakeBakeLocationDialogConfig(const FString& DefaultPackageName, ECambrianaBakeChoice Choice);

	/** Answers the live actor Bake's location dialog instead of showing it, for tests; unset, the dialog shows. Returns the previous prompt. */
	FBakeLocationPrompt SetBakeLocationPrompt(FBakeLocationPrompt Prompt);

	inline const FName BakeMenuSection(TEXT("GetAssetActions"));
	inline const FName BakeToStaticMeshMenuEntry(TEXT("CambrianaGrammar_BakeToStaticMesh"));
	inline const FName BakeToSkeletalMeshMenuEntry(TEXT("CambrianaGrammar_BakeToSkeletalMesh"));

	/**
	 * Bakes the grammar asset as it is in memory, saved or not, to a folder beside the asset named after it, and saves the outputs
	 * but not the grammar. StaticMesh writes SM_<grammar asset name>; SkeletalMesh writes the skeleton SKEL_<grammar asset name>, the
	 * skinned skeletal mesh SK_<grammar asset name> and its physics asset PHYS_<grammar asset name>; the static or skeletal mesh
	 * records the grammar in its import data for Reimport. AskToOverwrite is asked once before the chosen outputs that exist are
	 * rebuilt in place; unset, a message dialog asks.
	 */
	FCambrianaBakeResult BakeGrammar(UCambrianaGrammar& Grammar, ECambrianaBakeChoice Choice, const FAskToOverwrite& AskToOverwrite = nullptr,
		const FAskToReplace& AskToReplace = nullptr);

	/** Bakes as BakeGrammar does, building State, such as the grammar editor's working copy, into the outputs of Asset. */
	FCambrianaBakeResult BakeGrammarState(const UCambrianaGrammar& Asset, const UCambrianaGrammar& State, ECambrianaBakeChoice Choice,
		const FAskToOverwrite& AskToOverwrite = nullptr, const FAskToReplace& AskToReplace = nullptr);

	/**
	 * Rebuilds Mesh in place from the grammar asset as BakeGrammar does, without asking before: a static mesh as Bake to Static Mesh,
	 * a skeletal mesh as Bake to Skeletal Mesh with the grammar's bones merged into the skeleton it uses (SKEL_ beside the grammar
	 * when it has none), and its physics asset rebuilt only when that is the grammar's own PHYS_ beside it. When the bones do not
	 * merge, AskToRecreateSkeleton is asked, or unset a message dialog: Yes recreates the skeleton's bone tree, and No or Cancel
	 * rebuilds nothing. Links the mesh to the grammar, and leaves the rebuilt assets with unsaved changes.
	 */
	FCambrianaBakeResult RebakeMesh(UCambrianaGrammar& Grammar, UObject& Mesh, const FAskToRecreateSkeleton& AskToRecreateSkeleton = nullptr);

	/**
	 * Bakes Choice from the live actor's own build input, overrides included, and saves it where the engine's save-asset dialog says,
	 * which asks itself before an existing mesh of the class is replaced. The dialog defaults to SM_ (SK_ for SkeletalMesh) plus
	 * <grammar asset name>_<actor label> in the grammar's bake folder, or for an actor without a grammar asset plus <actor label> at
	 * the project's content root. A skeletal mesh's skeleton and physics asset go beside it, named from the chosen name with its SK_
	 * prefix replaced by SKEL_ and PHYS_, or with those put before a name without it; AskToOverwrite is asked once, naming the first
	 * of them that exists, before they are rebuilt in place, and unset a message dialog asks. The physics asset takes the grammar
	 * asset's Physics Stiffness, or the default for inline text. A mesh written over one linked to a grammar loses the link: its
	 * import data becomes plain import data with no source file.
	 */
	FCambrianaBakeResult BakeLiveActor(ACambrianaLiveActor& Actor, ECambrianaBakeChoice Choice, const FAskToOverwrite& AskToOverwrite = nullptr,
		const FAskToReplace& AskToReplace = nullptr);

	/** The label of Choice's menu entries and buttons: Bake to Static Mesh or Bake to Skeletal Mesh. */
	FText BakeChoiceLabel(ECambrianaBakeChoice Choice);

	/** The text of a bake result's notification; Source names what was baked. */
	FText BakeResultMessage(const FString& Source, const FCambrianaBakeResult& Result);

	/** Shows a notification of a bake's result; Source names what was baked. A declined overwrite or cancelled location shows nothing. */
	void NotifyBakeResult(const FString& Source, const FCambrianaBakeResult& Result);

	/** Runs a Bake choice from a menu entry. */
	using FExecuteBakeChoice = void (*)(const FToolMenuContext& MenuContext, ECambrianaBakeChoice Choice);

	/**
	 * Adds the Bake to Static Mesh or Bake to Skeletal Mesh entry for Choice, with the label and tooltip both the grammar asset's
	 * right-click menu and the grammar editor's Bake menu show, running Execute with Choice.
	 */
	void AddBakeChoiceEntry(FToolMenuSection& Section, FName Name, ECambrianaBakeChoice Choice, FExecuteBakeChoice Execute);

	/** Adds Bake to Static Mesh and Bake to Skeletal Mesh, in that order, to the grammar asset's right-click menu in the Content Browser, owned by the current tool menu owner. */
	void AddBakeMenuEntry();
}
