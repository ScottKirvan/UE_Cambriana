#include "CambrianaPreviewMaterials.h"

#include "CambrianaBuild.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName PreviewColorParameter(TEXT("Color"));
}

void Cambriana::ApplyPreviewMaterials(UDynamicMeshComponent& Component, TConstArrayView<int64> SlotMaterialIds,
	UMaterialInterface* BaseMaterial, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Palette, UObject* Outer)
{
	Palette.SetNum(PreviewPaletteSize);
	TArray<UMaterialInterface*> MaterialSet;
	MaterialSet.Reserve(SlotMaterialIds.Num());
	for (const int64 MaterialId : SlotMaterialIds)
	{
		TObjectPtr<UMaterialInstanceDynamic>& Material = Palette[PreviewPaletteIndex(MaterialId)];
		if (!Material && BaseMaterial)
		{
			Material = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
			Material->SetVectorParameterValue(PreviewColorParameter, PreviewPaletteColor(MaterialId));
		}
		MaterialSet.Add(Material);
	}
	Component.ConfigureMaterialSet(MaterialSet);
}
