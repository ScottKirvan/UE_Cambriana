#include "CambrianaSkeletalMesh.h"

#include "CambrianaBuild.h"

FReferenceSkeleton Cambriana::MakeReferenceSkeleton(const FCambrianaBuildResult& Result)
{
	return FReferenceSkeleton();
}

UE::AnimationCore::FBoneWeights Cambriana::MakeBoneWeights(TConstArrayView<FCambrianaInfluence> Influences)
{
	return UE::AnimationCore::FBoneWeights();
}

#if WITH_EDITOR
bool Cambriana::SetSkeletalMeshFromBuild(USkeletalMesh& SkeletalMesh, USkeleton& Skeleton, const FCambrianaBuildResult& Result)
{
	return false;
}
#endif
