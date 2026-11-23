#include "CambrianaPhysicsBodies.h"

#include "CambrianaBuild.h"
#include "CambrianaSkeletalMesh.h"

namespace
{
	bool IsDrawn(const FCambrianaBone& Bone)
	{
		return Bone.Kind == ECambrianaBoneKind::DrawnSegment;
	}

	/** The bone's capsule in component space: centred on the bone, its Z axis along the bone, as an FKSphylElem lies along its Z axis. */
	FTransform CapsulePose(const FCambrianaBone& Bone)
	{
		return FTransform(FRotationMatrix::MakeFromZX(Bone.Forward, Bone.Up).ToQuat(), (Bone.Start + Bone.End) * 0.5);
	}

	/** The nearest drawn bone above the bone, or INDEX_NONE. A parent comes before its child, so walking up only visits lower indices and ends. */
	int32 NearestDrawnAncestor(TConstArrayView<FCambrianaBone> Bones, int32 Index)
	{
		for (int32 Ancestor = Bones[Index].Parent; Ancestor >= 0 && Ancestor < Index; Index = Ancestor, Ancestor = Bones[Ancestor].Parent)
		{
			if (IsDrawn(Bones[Ancestor]))
			{
				return Ancestor;
			}
		}
		return INDEX_NONE;
	}
}

TArray<FCambrianaPhysicsBody> Cambriana::MakePhysicsBodies(const FCambrianaBuildResult& Result)
{
	const TConstArrayView<FCambrianaBone> Bones = Result.Bones;
	TArray<FCambrianaPhysicsBody> Bodies;
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		const FCambrianaBone& Bone = Bones[Index];
		if (!IsDrawn(Bone))
		{
			continue;
		}
		FCambrianaPhysicsBody& Body = Bodies.AddDefaulted_GetRef();
		Body.SkeletonBone = Index + 1;
		const int32 ParentBody = NearestDrawnAncestor(Bones, Index);
		Body.ParentBodyBone = ParentBody == INDEX_NONE ? INDEX_NONE : ParentBody + 1;

		const double Length = FVector3d::Distance(Bone.Start, Bone.End);
		FKSphylElem& Capsule = Body.Capsules.Emplace_GetRef(float(Bone.Radius), float(Length));
		Capsule.SetTransform(CapsulePose(Bone).GetRelativeTransform(BoneBindPose(Bone)));
		// A bone without thickness or length has no stiffness.
		Body.BendingStiffness = Bone.Radius > 0.0 && Length > 0.0 ? FMath::Pow(Bone.Radius, 4.0) / Length : 0.0;
	}
	return Bodies;
}
