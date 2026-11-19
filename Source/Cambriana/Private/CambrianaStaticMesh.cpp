#include "CambrianaStaticMesh.h"

#include "Algo/Transform.h"
#include "CambrianaBuild.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace
{
	// UV0 to UV2 carry the surface UVs and data channels (SDD-surfacing.md, Data channels); lightmap UVs go after them.
	constexpr int32 LightmapUVChannel = 3;

	FMeshDescription MakeMeshDescription(const FCambrianaBuildResult& Result, TConstArrayView<FName> SlotNames)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		FDynamicMeshToMeshDescription Converter(FConversionToMeshDescriptionOptions{});
		Converter.Convert(&Result.Mesh, Description);

		// The converter makes polygon group N for material ID N, and each triangle's material ID is its slot index.
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

	TArray<FStaticMaterial> MakeSlots(const UStaticMesh& StaticMesh, TConstArrayView<FName> SlotNames)
	{
		const TArray<FStaticMaterial>& Existing = StaticMesh.GetStaticMaterials();
		TArray<FStaticMaterial> Slots;
		Slots.Reserve(SlotNames.Num());
		for (const FName SlotName : SlotNames)
		{
			const FStaticMaterial* Kept = Existing.FindByPredicate([SlotName](const FStaticMaterial& Slot) { return Slot.MaterialSlotName == SlotName; });
			FStaticMaterial& Slot = Slots.Emplace_GetRef(Kept ? Kept->MaterialInterface.Get() : UMaterial::GetDefaultMaterial(MD_Surface), SlotName, SlotName);
			Slot.UVChannelData = FMeshUVChannelInfo(1.0f);
		}
		return Slots;
	}
}

FName Cambriana::MaterialSlotName(int64 MaterialId)
{
	return FName(*FString::Printf(TEXT("Material_%lld"), MaterialId));
}

void Cambriana::SetStaticMeshFromBuild(UStaticMesh& StaticMesh, const FCambrianaBuildResult& Result)
{
	TArray<FName> SlotNames;
	Algo::Transform(Result.SlotMaterialIds, SlotNames, &MaterialSlotName);
	FMeshDescription Description = MakeMeshDescription(Result, SlotNames);
	StaticMesh.SetStaticMaterials(MakeSlots(StaticMesh, SlotNames));

#if WITH_EDITOR
	StaticMesh.SetNumSourceModels(1);
	FMeshBuildSettings& BuildSettings = StaticMesh.GetSourceModel(0).BuildSettings;
	BuildSettings.bRecomputeNormals = false;
	BuildSettings.bRecomputeTangents = true;
	BuildSettings.bGenerateLightmapUVs = true;
	BuildSettings.SrcLightmapIndex = 0;
	BuildSettings.DstLightmapIndex = LightmapUVChannel;
	StaticMesh.SetLightMapCoordinateIndex(LightmapUVChannel);

	StaticMesh.CreateMeshDescription(0, MoveTemp(Description));
	StaticMesh.CommitMeshDescription(0);
	FMeshSectionInfoMap& Sections = StaticMesh.GetSectionInfoMap();
	Sections.Clear();
	for (int32 Slot = 0; Slot < SlotNames.Num(); ++Slot)
	{
		Sections.Set(0, Slot, FMeshSectionInfo(Slot));
	}
	StaticMesh.GetOriginalSectionInfoMap().CopyFrom(Sections);
	StaticMesh.PostEditChange();
#else
	StaticMesh.BuildFromMeshDescriptions({ &Description });
#endif
}
