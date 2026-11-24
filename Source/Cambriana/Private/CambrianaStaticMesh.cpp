#include "CambrianaStaticMesh.h"

#include "Algo/Transform.h"
#include "CambrianaBuild.h"
#include "CambrianaMaterialSlots.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/StaticMesh.h"
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
		// The static mesh build stores vertex colours as sRGB-encoded bytes. Converting the channel values sRGB to linear first
		// cancels that, so a material's Vertex Color reads the values themselves, as mesh paint does
		// (FDynamicMeshToMeshDescription::ApplyVertexColorTransform).
		FConversionToMeshDescriptionOptions Options;
		Options.bTransformVtxColorsSRGBToLinear = true;
		FDynamicMeshToMeshDescription Converter(Options);
		Converter.Convert(&Result.Mesh, Description);

		Cambriana::Private::NamePolygonGroupSlots(Description, SlotNames);
		return Description;
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
	StaticMesh.SetStaticMaterials(Cambriana::Private::MakeMaterialSlots<FStaticMaterial>(StaticMesh.GetStaticMaterials(), SlotNames));

#if WITH_EDITOR
	StaticMesh.SetNumSourceModels(1);
	FMeshBuildSettings& BuildSettings = StaticMesh.GetSourceModel(0).BuildSettings;
	BuildSettings.bRecomputeNormals = false;
	BuildSettings.bRecomputeTangents = true;
	// Keeps the build's sliver triangles, as Geometry Script's CopyMeshToStaticMesh does by default (bEnableRemoveDegenerates).
	BuildSettings.bRemoveDegenerates = false;
	// UV2's root distance is in cm, which half precision holds at 200 cm only to 0.125 cm. Datasmith's static mesh import and
	// the groom deformer builder, which also carry data in UVs, store them at full precision too.
	BuildSettings.bUseFullPrecisionUVs = true;
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
