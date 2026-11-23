#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/SphylElem.h"

struct FCambrianaBuildResult;

/** A physics body of a baked mesh: one per drawn bone, holding one capsule along it. */
struct FCambrianaPhysicsBody
{
	/** The reference skeleton bone the body is on (Cambriana::MakeReferenceSkeleton). */
	int32 SkeletonBone = INDEX_NONE;
	/** The reference skeleton bone of the nearest body above this one, or INDEX_NONE for a body with none above it. */
	int32 ParentBodyBone = INDEX_NONE;
	/** The bone's capsule, in the space of the body's bone: along the bone, its length and radius. */
	TArray<FKSphylElem> Capsules;
	/** The beam bending model's stiffness of the body's bone without its material constant, radius⁴ / length, in cm³. */
	double BendingStiffness = 0.0;
};

namespace Cambriana
{
	/** The physics bodies of a build: one per drawn bone, in bone order. */
	CAMBRIANA_API TArray<FCambrianaPhysicsBody> MakePhysicsBodies(const FCambrianaBuildResult& Result);
}
