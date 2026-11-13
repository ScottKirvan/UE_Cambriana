#pragma once

#include "BoneWeights.h"
#include "CoreMinimal.h"
#include "ReferenceSkeleton.h"

class USkeletalMesh;
class USkeleton;
struct FCambrianaBuildResult;
struct FCambrianaInfluence;

namespace Cambriana
{
	/** The synthetic bone at the origin that parents the build's bone tree (SDD-rigging.md, Bake skeleton). */
	inline const FName RootBoneName(TEXT("Root"));

	/**
	 * The reference skeleton of a build: Root at index 0, then each bone of the build at its index plus one, its roots parented to
	 * Root. The bind pose is the bones' own frames: each bone at its start, X along the bone, Z up. Empty when the bone tree cannot
	 * make one (a parent after its child, or two bones of one name); the problem is logged.
	 */
	CAMBRIANA_API FReferenceSkeleton MakeReferenceSkeleton(const FCambrianaBuildResult& Result);

	/**
	 * A vertex's skin weights on the reference skeleton from its influences on the build's bones, with the engine's per-vertex
	 * influence limit applied: the strongest influences kept and renormalised. A vertex without influences is bound to Root.
	 */
	CAMBRIANA_API UE::AnimationCore::FBoneWeights MakeBoneWeights(TConstArrayView<FCambrianaInfluence> Influences);

#if WITH_EDITOR
	/**
	 * Makes SkeletalMesh a single LOD holding the build result's mesh, skinned to the build's reference skeleton, with one
	 * material slot per material ID as for a static mesh, and makes Skeleton that mesh's skeleton with the same bones. A slot keeps
	 * the material of the skeletal mesh's existing slot of the same name. Returns false, changing nothing, when the bone tree cannot
	 * make a reference skeleton.
	 */
	CAMBRIANA_API bool SetSkeletalMeshFromBuild(USkeletalMesh& SkeletalMesh, USkeleton& Skeleton, const FCambrianaBuildResult& Result);
#endif
}
