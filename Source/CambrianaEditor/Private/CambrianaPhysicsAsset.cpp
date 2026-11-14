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
	/** FConstraintDrive's default damping to stiffness (ConstraintDrives.cpp: stiffness 50, damping 1). */
	constexpr float DampingPerStiffness = 1.0f / 50.0f;

	/**
	 * Drives the constraint back to the bind pose its frames are snapped to, as USkeletalMeshComponent::SetAllMotorsAngularPositionDrive
	 * and SetAllMotorsAngularDriveParams set the drives of a skeletal mesh's constraints. SLERP, the engine's default angular drive mode
	 * (FAngularDriveConstraint), with its velocity drive on as well so that the damping applies, as FPhysicsControlRecord sets it
	 * (PhysicsControlRecord.cpp). Force mode, so that the stiffness is a torque per angle, as in the beam bending model, rather than
	 * scaled by the body's inertia.
	 */
	void DriveToBindPose(FConstraintInstance& Constraint, float Stiffness)
	{
		Constraint.SetAngularDriveMode(EAngularDriveMode::SLERP);
		Constraint.SetOrientationDriveSLERP(true);
		Constraint.SetAngularVelocityDriveSLERP(true);
		Constraint.SetAngularDriveAccelerationMode(false);
		Constraint.SetAngularOrientationTarget(FQuat::Identity);
		Constraint.SetAngularVelocityTarget(FVector::ZeroVector);
		Constraint.SetAngularDriveParams(Stiffness, Stiffness * DampingPerStiffness, 0.0f);
	}

	/** A constraint from a body to the body above it, as FPhysicsAssetUtils::CreateFromSkeletalMesh joins each body to its parent's. */
	void ConstrainToParent(UPhysicsAsset& PhysicsAsset, const FPhysAssetCreateParams& Params, FName Bone, int32 Body, FName ParentBone, float Stiffness)
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
		DriveToBindPose(Constraint->DefaultInstance, Stiffness);
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
			ConstrainToParent(PhysicsAsset, Params, Bone, BodyIndex, Skeleton.GetBoneName(Body.ParentBodyBone), float(Stiffness * Body.BendingStiffness));
		}
	}
	PhysicsAsset.UpdateBodySetupIndexMap();
	PhysicsAsset.UpdateBoundsBodiesArray();
	SkeletalMesh.SetPhysicsAsset(&PhysicsAsset);
}
