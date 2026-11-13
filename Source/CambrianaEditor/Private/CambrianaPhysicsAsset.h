#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalMesh;
struct FCambrianaBuildResult;

namespace Cambriana::Editor
{
	/**
	 * Makes PhysicsAsset the physics asset of SkeletalMesh, built from the same build result: one body per chain of the bone tree on
	 * the chain's first bone, holding a capsule per bone of the chain, and a constraint from each body to the nearest body above it.
	 * Its earlier bodies and constraints are replaced.
	 */
	void SetPhysicsAssetFromBuild(UPhysicsAsset& PhysicsAsset, USkeletalMesh& SkeletalMesh, const FCambrianaBuildResult& Result);
}
