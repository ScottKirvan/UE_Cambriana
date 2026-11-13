#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/SphylElem.h"

struct FCambrianaBone;
struct FCambrianaBuildResult;

/** A physics body of a baked mesh: one per chain, on the chain's first bone, holding a capsule per bone of the chain. */
struct FCambrianaPhysicsBody
{
	/** The reference skeleton bone the body is on (Cambriana::MakeReferenceSkeleton). */
	int32 SkeletonBone = INDEX_NONE;
	/** The reference skeleton bone of the nearest body above this one, or INDEX_NONE for a body with none above it. */
	int32 ParentBodyBone = INDEX_NONE;
	/** One capsule per bone of the chain, in the space of the body's bone: along the bone, its length and radius. */
	TArray<FKSphylElem> Capsules;
};

namespace Cambriana
{
	/**
	 * The chains of a bone tree, each the build bone indices of a chain from its first bone. A chain is a maximal run of drawn segments,
	 * each after the first the only drawn child of the one before it, with no chain break before it.
	 */
	CAMBRIANA_API TArray<TArray<int32>> FindChains(TConstArrayView<FCambrianaBone> Bones);

	/** The physics bodies of a build: one per chain, in chain order. */
	CAMBRIANA_API TArray<FCambrianaPhysicsBody> MakePhysicsBodies(const FCambrianaBuildResult& Result);
}
