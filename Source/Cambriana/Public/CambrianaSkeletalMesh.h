#pragma once

#include "BoneWeights.h"
#include "CoreMinimal.h"
#include "ReferenceSkeleton.h"

class USkeletalMesh;
class USkeleton;
struct FCambrianaBone;
struct FCambrianaBuildResult;
struct FCambrianaInfluence;

namespace Cambriana
{
	/** The synthetic bone at the origin that parents the build's bone tree (SDD-rigging.md, Bake skeleton). */
	inline const FName RootBoneName(TEXT("Root"));

	/** A bone's bind pose in component space: at its start, X along the bone and Z up. */
	CAMBRIANA_API FTransform BoneBindPose(const FCambrianaBone& Bone);

	/**
	 * The reference skeleton of a build: Root at index 0, then each bone of the build at its index plus one, its roots parented to
	 * Root. The bind pose is the bones' own frames: each bone at its start, X along the bone, Z up. Empty when the bone tree cannot
	 * make one (a parent after its child, or two bones of one name); the problem is logged. Made against Skeleton, when given, as the
	 * engine's reference skeleton modifier makes it, so that it carries that skeleton's virtual bones.
	 */
	CAMBRIANA_API FReferenceSkeleton MakeReferenceSkeleton(const FCambrianaBuildResult& Result, const USkeleton* Skeleton = nullptr);

	/** How SetSkeletalMeshFromBuild gives the skeleton the mesh's bones. */
	enum class ECambrianaSkeletonUpdate : uint8
	{
		/** The skeleton's bone tree is recreated from the mesh's bones. */
		RecreateBoneTree,
		/** The mesh's bones are merged into the skeleton's, which keeps every bone it has. */
		MergeBones,
	};

	/**
	 * A vertex's skin weights on the reference skeleton from its influences on the build's bones, with the engine's per-vertex
	 * influence limit applied: the strongest influences kept and renormalised. A vertex without influences is bound to Root.
	 */
	CAMBRIANA_API UE::AnimationCore::FBoneWeights MakeBoneWeights(TConstArrayView<FCambrianaInfluence> Influences);

#if WITH_EDITOR
	/**
	 * Makes SkeletalMesh a single LOD holding the build result's mesh, skinned to the build's reference skeleton, with one
	 * material slot per material ID as for a static mesh, and makes Skeleton that mesh's skeleton, its bone tree recreated with the
	 * same bones or, for MergeBones, with the mesh's bones merged into it. A slot keeps the material of the skeletal mesh's existing
	 * slot of the same name. Returns false, changing nothing, when the bone tree cannot make a reference skeleton, and false with
	 * the mesh rebuilt when the bones do not merge.
	 */
	CAMBRIANA_API bool SetSkeletalMeshFromBuild(USkeletalMesh& SkeletalMesh, USkeleton& Skeleton, const FCambrianaBuildResult& Result,
		ECambrianaSkeletonUpdate Update = ECambrianaSkeletonUpdate::RecreateBoneTree);
#endif
}
