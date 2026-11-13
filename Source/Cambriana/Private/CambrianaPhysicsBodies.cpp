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

	/** The chain each bone is in, or INDEX_NONE for a bone in none. */
	TArray<int32> ChainOfEachBone(TConstArrayView<FCambrianaBone> Bones, TConstArrayView<TArray<int32>> Chains)
	{
		TArray<int32> ChainOf;
		ChainOf.Init(INDEX_NONE, Bones.Num());
		for (int32 Chain = 0; Chain < Chains.Num(); ++Chain)
		{
			for (const int32 Bone : Chains[Chain])
			{
				ChainOf[Bone] = Chain;
			}
		}
		return ChainOf;
	}
}

TArray<TArray<int32>> Cambriana::FindChains(TConstArrayView<FCambrianaBone> Bones)
{
	TArray<int32> DrawnChildren;
	DrawnChildren.Init(0, Bones.Num());
	for (const FCambrianaBone& Bone : Bones)
	{
		if (IsDrawn(Bone) && Bones.IsValidIndex(Bone.Parent))
		{
			++DrawnChildren[Bone.Parent];
		}
	}

	TArray<TArray<int32>> Chains;
	TArray<int32> ChainOf;
	ChainOf.Init(INDEX_NONE, Bones.Num());
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		const FCambrianaBone& Bone = Bones[Index];
		if (!IsDrawn(Bone))
		{
			continue;
		}
		const int32 Parent = Bone.Parent;
		const bool bContinues = Parent >= 0 && Parent < Index && ChainOf[Parent] != INDEX_NONE && DrawnChildren[Parent] == 1 && !Bone.bChainBreakBefore;
		if (bContinues)
		{
			ChainOf[Index] = ChainOf[Parent];
			Chains[ChainOf[Index]].Add(Index);
		}
		else
		{
			ChainOf[Index] = Chains.Add({ Index });
		}
	}
	return Chains;
}

TArray<FCambrianaPhysicsBody> Cambriana::MakePhysicsBodies(const FCambrianaBuildResult& Result)
{
	const TConstArrayView<FCambrianaBone> Bones = Result.Bones;
	const TArray<TArray<int32>> Chains = FindChains(Bones);
	const TArray<int32> ChainOf = ChainOfEachBone(Bones, Chains);

	TArray<FCambrianaPhysicsBody> Bodies;
	Bodies.Reserve(Chains.Num());
	for (const TArray<int32>& Chain : Chains)
	{
		const int32 First = Chain[0];
		FCambrianaPhysicsBody& Body = Bodies.AddDefaulted_GetRef();
		Body.SkeletonBone = First + 1;
		// A parent comes before its child, so walking up only visits lower indices and ends.
		for (int32 Ancestor = Bones[First].Parent; Ancestor >= 0 && Ancestor < First; Ancestor = Bones[Ancestor].Parent < Ancestor ? Bones[Ancestor].Parent : INDEX_NONE)
		{
			if (ChainOf[Ancestor] != INDEX_NONE)
			{
				Body.ParentBodyBone = Chains[ChainOf[Ancestor]][0] + 1;
				break;
			}
		}

		const FTransform BodyPose = BoneBindPose(Bones[First]);
		for (const int32 Index : Chain)
		{
			const FCambrianaBone& Bone = Bones[Index];
			FKSphylElem& Capsule = Body.Capsules.Emplace_GetRef(float(Bone.Radius), float(FVector3d::Distance(Bone.Start, Bone.End)));
			Capsule.SetTransform(CapsulePose(Bone).GetRelativeTransform(BodyPose));
		}
	}
	return Bodies;
}
