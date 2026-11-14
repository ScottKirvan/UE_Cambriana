#include "CambrianaPhysicsAsset.h"

#include "CambrianaBuild.h"
#include "CambrianaPhysicsBodies.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

namespace
{
	/** A constraint from a body to the body above it, as FPhysicsAssetUtils::CreateFromSkeletalMesh joins each body to its parent's. */
	void ConstrainToParent(UPhysicsAsset& PhysicsAsset, const FPhysAssetCreateParams& Params, FName Bone, int32 Body, FName ParentBone)
	{
		const int32 ParentBody = PhysicsAsset.FindBodyIndex(ParentBone);
		if (ParentBody == INDEX_NONE || !FPhysicsAssetUtils::CanCreateConstraints())
		{
			return;
		}
		UPhysicsConstraintTemplate* Constraint = PhysicsAsset.ConstraintSetup[FPhysicsAssetUtils::CreateNewConstraint(&PhysicsAsset, Bone)];
		Constraint->DefaultInstance.SetAngularSwing1Motion(Params.AngularConstraintMode);
		Constraint->DefaultInstance.SetAngularSwing2Motion(Params.AngularConstraintMode);
		Constraint->DefaultInstance.SetAngularTwistMotion(Params.AngularConstraintMode);
		Constraint->DefaultInstance.ConstraintBone1 = Bone;
		Constraint->DefaultInstance.ConstraintBone2 = ParentBone;
		Constraint->DefaultInstance.SnapTransformsToDefault(EConstraintTransformComponentFlags::All, &PhysicsAsset);
		Constraint->SetDefaultProfile(Constraint->DefaultInstance);
		PhysicsAsset.DisableCollision(Body, ParentBody);
	}
}

void Cambriana::Editor::SetPhysicsAssetFromBuild(UPhysicsAsset& PhysicsAsset, USkeletalMesh& SkeletalMesh, const FCambrianaBuildResult& Result, float Stiffness)
{
	// Earlier bodies are emptied as the physics asset editor empties them before regenerating all bodies (PhysicsAssetEditor.cpp).
	PhysicsAsset.SkeletalBodySetups.Empty();
	PhysicsAsset.BodySetupIndexMap.Empty();
	PhysicsAsset.ConstraintSetup.Empty();
	PhysicsAsset.CollisionDisableTable.Empty();
	PhysicsAsset.PreviewSkeletalMesh = &SkeletalMesh;

	// The FBX skeletal mesh import's creation parameters (FbxSkeletalMeshImport.cpp): capsules, limited angular constraints between
	// bodies, and collision between bodies off by default.
	const FPhysAssetCreateParams Params;
	const FReferenceSkeleton& Skeleton = SkeletalMesh.GetRefSkeleton();
	for (const FCambrianaPhysicsBody& Body : Cambriana::MakePhysicsBodies(Result))
	{
		const FName Bone = Skeleton.GetBoneName(Body.SkeletonBone);
		const int32 BodyIndex = FPhysicsAssetUtils::CreateNewBody(&PhysicsAsset, Bone, Params);
		USkeletalBodySetup* Setup = PhysicsAsset.SkeletalBodySetups[BodyIndex];
		Setup->RemoveSimpleCollision();
		Setup->AggGeom.SphylElems = Body.Capsules;
		Setup->InvalidatePhysicsData();
		if (Body.ParentBodyBone != INDEX_NONE && Params.bCreateConstraints)
		{
			ConstrainToParent(PhysicsAsset, Params, Bone, BodyIndex, Skeleton.GetBoneName(Body.ParentBodyBone));
		}
	}
	PhysicsAsset.UpdateBodySetupIndexMap();
	PhysicsAsset.UpdateBoundsBodiesArray();
	SkeletalMesh.SetPhysicsAsset(&PhysicsAsset);
}
