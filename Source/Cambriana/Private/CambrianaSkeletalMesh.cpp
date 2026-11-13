#include "CambrianaSkeletalMesh.h"

#include "Algo/Transform.h"
#include "CambrianaBuild.h"
#include "CambrianaCoreLoader.h"
#include "CambrianaStaticMesh.h"

#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "RenderingThread.h"
#include "SkeletalMeshAttributes.h"
#endif

namespace
{
	using UE::AnimationCore::FBoneWeight;
	using UE::AnimationCore::FBoneWeights;

	/** Whether the bone tree can make a reference skeleton: every parent before its child, and every name used once, Root's included. */
	bool CanMakeReferenceSkeleton(TConstArrayView<FCambrianaBone> Bones)
	{
		TSet<FName> Names = { Cambriana::RootBoneName };
		for (int32 Index = 0; Index < Bones.Num(); ++Index)
		{
			const FCambrianaBone& Bone = Bones[Index];
			if (Bone.Parent >= Index)
			{
				UE_LOG(LogCambriana, Error, TEXT("Bone %d (%s) comes before its parent %d; no skeleton is made"), Index, *Bone.Name.ToString(), Bone.Parent);
				return false;
			}
			bool bAlreadyUsed = false;
			Names.Add(Bone.Name, &bAlreadyUsed);
			if (bAlreadyUsed || Bone.Name.IsNone())
			{
				UE_LOG(LogCambriana, Error, TEXT("Bone %d's name '%s' is not unique; no skeleton is made"), Index, *Bone.Name.ToString());
				return false;
			}
		}
		return true;
	}

	/** The bone's bind pose in component space: at its start, X along the bone and Z up, as Control Rig's aim aims X with Z secondary. */
	FTransform BindPose(const FCambrianaBone& Bone)
	{
		return FTransform(FRotationMatrix::MakeFromXZ(Bone.Forward, Bone.Up).ToQuat(), Bone.Start);
	}

	/** Each bone's bind pose relative to its parent in the reference skeleton, Root first. */
	TArray<FTransform> LocalBindPoses(TConstArrayView<FCambrianaBone> Bones)
	{
		TArray<FTransform> Component;
		TArray<FTransform> Local;
		Component.Reserve(Bones.Num());
		Local.Reserve(Bones.Num() + 1);
		Local.Add(FTransform::Identity);
		for (const FCambrianaBone& Bone : Bones)
		{
			const FTransform& Pose = Component.Add_GetRef(BindPose(Bone));
			Local.Add(Bone.Parent == INDEX_NONE ? Pose : Pose.GetRelativeTransform(Component[Bone.Parent]));
		}
		return Local;
	}

	int32 SkeletonParent(const FCambrianaBone& Bone)
	{
		return Bone.Parent == INDEX_NONE ? 0 : Bone.Parent + 1;
	}

#if WITH_EDITOR
	/** The build's mesh with the reference skeleton's bones and each vertex's skin weights, as Geometry Script's skinned meshes carry them. */
	UE::Geometry::FDynamicMesh3 MakeSkinnedMesh(const FCambrianaBuildResult& Result, const FReferenceSkeleton& Skeleton)
	{
		UE::Geometry::FDynamicMesh3 Mesh = Result.Mesh;
		UE::Geometry::FDynamicMeshAttributeSet& Attributes = *Mesh.Attributes();
		Attributes.EnableBones(Skeleton.GetRawBoneNum());
		for (int32 Bone = 0; Bone < Skeleton.GetRawBoneNum(); ++Bone)
		{
			Attributes.GetBoneNames()->SetValue(Bone, Skeleton.GetBoneName(Bone));
			Attributes.GetBoneParentIndices()->SetValue(Bone, Skeleton.GetParentIndex(Bone));
			Attributes.GetBonePoses()->SetValue(Bone, Skeleton.GetRefBonePose()[Bone]);
		}

		auto* SkinWeights = new UE::Geometry::FDynamicMeshVertexSkinWeightsAttribute(&Mesh);
		for (const int32 Vid : Mesh.VertexIndicesItr())
		{
			SkinWeights->SetValue(Vid, Cambriana::MakeBoneWeights(Result.VertexInfluences.IsValidIndex(Vid) ? Result.VertexInfluences[Vid] : FCambrianaVertexInfluences()));
		}
		Attributes.AttachSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName, SkinWeights);
		return Mesh;
	}

	FMeshDescription MakeMeshDescription(const FCambrianaBuildResult& Result, const FReferenceSkeleton& Skeleton, TConstArrayView<FName> SlotNames)
	{
		FMeshDescription Description;
		FSkeletalMeshAttributes Attributes(Description);
		Attributes.Register();
		// Vertex colours are kept linear: the skeletal mesh build stores them without sRGB encoding (SkeletalMeshLODImporterData.cpp,
		// "avoid linear to srgb conversion"), so the static mesh's sRGB-to-linear transform is not applied
		// (StaticToSkeletalMeshConverter.cpp undoes it for the same reason).
		FDynamicMeshToMeshDescription Converter;
		const UE::Geometry::FDynamicMesh3 Skinned = MakeSkinnedMesh(Result, Skeleton);
		Converter.Convert(&Skinned, Description);

		TPolygonGroupAttributesRef<FName> GroupSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
		{
			if (SlotNames.IsValidIndex(Group.GetValue()))
			{
				GroupSlotNames[Group] = SlotNames[Group.GetValue()];
			}
		}
		return Description;
	}

	TArray<FSkeletalMaterial> MakeSlots(const USkeletalMesh& SkeletalMesh, TConstArrayView<FName> SlotNames)
	{
		const TArray<FSkeletalMaterial>& Existing = SkeletalMesh.GetMaterials();
		TArray<FSkeletalMaterial> Slots;
		Slots.Reserve(SlotNames.Num());
		for (const FName SlotName : SlotNames)
		{
			const FSkeletalMaterial* Kept = Existing.FindByPredicate([SlotName](const FSkeletalMaterial& Slot) { return Slot.MaterialSlotName == SlotName; });
			FSkeletalMaterial& Slot = Slots.Emplace_GetRef(Kept ? Kept->MaterialInterface.Get() : UMaterial::GetDefaultMaterial(MD_Surface), SlotName, SlotName);
			Slot.UVChannelData = FMeshUVChannelInfo(1.0f);
		}
		return Slots;
	}

	void SetBuildSettings(FSkeletalMeshLODInfo& Lod)
	{
		FSkeletalMeshBuildSettings& Settings = Lod.BuildSettings;
		Settings.bRecomputeNormals = false;
		Settings.bRecomputeTangents = true;
		// As for the static mesh (CambrianaStaticMesh.cpp): sliver triangles are kept, and UVs carry data channels at full precision.
		Settings.bRemoveDegenerates = false;
		Settings.bUseFullPrecisionUVs = true;
		// Geometry Script's CopyMeshToSkeletalMesh prevents decimation of the LOD it writes.
		Lod.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
		Lod.ReductionSettings.NumOfVertPercentage = 1.0f;
		Lod.ReductionSettings.MaxNumOfTriangles = MAX_uint32;
		Lod.ReductionSettings.MaxNumOfVerts = MAX_uint32;
		Lod.ReductionSettings.BaseLOD = 0;
		Lod.LODMaterialMap.Reset();
	}
#endif
}

FReferenceSkeleton Cambriana::MakeReferenceSkeleton(const FCambrianaBuildResult& Result)
{
	FReferenceSkeleton Skeleton;
	if (!CanMakeReferenceSkeleton(Result.Bones))
	{
		return Skeleton;
	}
	const TArray<FTransform> Poses = LocalBindPoses(Result.Bones);
	{
		// The modifier completes the skeleton when it goes out of scope.
		FReferenceSkeletonModifier Modifier(Skeleton, nullptr);
		Modifier.Add(FMeshBoneInfo(RootBoneName, RootBoneName.ToString(), INDEX_NONE), Poses[0]);
		for (int32 Index = 0; Index < Result.Bones.Num(); ++Index)
		{
			const FCambrianaBone& Bone = Result.Bones[Index];
			Modifier.Add(FMeshBoneInfo(Bone.Name, Bone.Name.ToString(), SkeletonParent(Bone)), Poses[Index + 1]);
		}
	}
	return Skeleton;
}

UE::AnimationCore::FBoneWeights Cambriana::MakeBoneWeights(TConstArrayView<FCambrianaInfluence> Influences)
{
	// As the engine's skeletal mesh import processes influences (LODUtilities.cpp, FLODUtilities::ProcessImportMeshInfluences): the
	// weights normalised, the strongest MAX_TOTAL_INFLUENCES above BoneWeightThreshold kept and renormalised, and a vertex left
	// without any bound to bone 0, here Root.
	TArray<FCambrianaInfluence, TInlineAllocator<UE::AnimationCore::MaxInlineBoneWeightCount>> Kept;
	double Total = 0.0;
	for (const FCambrianaInfluence& Influence : Influences)
	{
		if (Influence.Bone >= 0 && Influence.Bone < MAX_uint16 && Influence.Weight > 0.0)
		{
			Kept.Add(Influence);
			Total += Influence.Weight;
		}
	}
	Kept.StableSort([](const FCambrianaInfluence& A, const FCambrianaInfluence& B) { return A.Weight > B.Weight; });
	Kept.RemoveAll([Total](const FCambrianaInfluence& Influence) { return Influence.Weight / Total < UE::AnimationCore::BoneWeightThreshold; });
	Kept.SetNum(FMath::Min(Kept.Num(), MAX_TOTAL_INFLUENCES));

	double KeptTotal = 0.0;
	for (const FCambrianaInfluence& Influence : Kept)
	{
		KeptTotal += Influence.Weight;
	}
	TArray<FBoneWeight, TInlineAllocator<UE::AnimationCore::MaxInlineBoneWeightCount>> Weights;
	for (const FCambrianaInfluence& Influence : Kept)
	{
		Weights.Emplace(FBoneIndexType(Influence.Bone + 1), float(Influence.Weight / KeptTotal));
	}
	UE::AnimationCore::FBoneWeightsSettings Settings;
	Settings.SetDefaultBoneIndex(0);
	return FBoneWeights::Create(Weights, Settings);
}

#if WITH_EDITOR
bool Cambriana::SetSkeletalMeshFromBuild(USkeletalMesh& SkeletalMesh, USkeleton& Skeleton, const FCambrianaBuildResult& Result)
{
	const FReferenceSkeleton ReferenceSkeleton = MakeReferenceSkeleton(Result);
	if (ReferenceSkeleton.GetRawBoneNum() == 0)
	{
		return false;
	}
	TArray<FName> SlotNames;
	Algo::Transform(Result.SlotMaterialIds, SlotNames, &MaterialSlotName);
	FMeshDescription Description = MakeMeshDescription(Result, ReferenceSkeleton, SlotNames);

	// The sequence of Geometry Script's CopyMeshToSkeletalMesh (MeshAssetFunctions.cpp) with a new reference skeleton.
	FlushRenderingCommands();
	SkeletalMesh.PreEditChange(nullptr);
	if (SkeletalMesh.GetLODNum() == 0)
	{
		SkeletalMesh.AddLODInfo();
		SkeletalMesh.GetImportedModel()->LODModels.Add(new FSkeletalMeshLODModel);
	}
	SkeletalMesh.SetRefSkeleton(ReferenceSkeleton);
	SkeletalMesh.CalculateInvRefMatrices();
	SkeletalMesh.SetMaterials(MakeSlots(SkeletalMesh, SlotNames));
	SetBuildSettings(*SkeletalMesh.GetLODInfo(0));
	SkeletalMesh.CreateMeshDescription(0, MoveTemp(Description));
	SkeletalMesh.CommitMeshDescription(0);
	// UV0 to UV2 and the vertex colour carry data channels (SDD-surfacing.md, Data channels).
	SkeletalMesh.SetHasVertexColors(true);
	SkeletalMesh.SetVertexColorGuid(FGuid::NewGuid());

	// As the procedural vegetation export makes a skeleton for the skeletal mesh it writes (PVExportHelper.cpp,
	// ExportCollectionToSkeletalMesh): the mesh's skeleton is set and its bone tree recreated from the mesh, which also replaces the
	// tree of a skeleton rebuilt in place.
	SkeletalMesh.SetSkeleton(&Skeleton);
	Skeleton.RecreateBoneTree(&SkeletalMesh);
	SkeletalMesh.PostEditChange();
	return true;
}
#endif
