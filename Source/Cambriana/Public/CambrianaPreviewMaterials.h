#pragma once

#include "CoreMinimal.h"

class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

namespace Cambriana
{
	inline constexpr const TCHAR* PreviewBaseMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	/**
	 * Gives each material slot of the component the preview palette colour of its core material ID. Palette holds one
	 * instance of BaseMaterial per palette colour, made on first use and outered to Outer.
	 */
	CAMBRIANA_API void ApplyPreviewMaterials(UDynamicMeshComponent& Component, TConstArrayView<int64> SlotMaterialIds,
		UMaterialInterface* BaseMaterial, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Palette, UObject* Outer);
}
