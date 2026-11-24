#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace Cambriana::Private
{
	/**
	 * One slot per name for a static or skeletal mesh (FStaticMaterial or FSkeletalMaterial). A slot keeps the material of the
	 * existing slot of the same name; a new one holds the engine's default surface material.
	 */
	template <typename SlotType>
	TArray<SlotType> MakeMaterialSlots(TConstArrayView<SlotType> Existing, TConstArrayView<FName> SlotNames)
	{
		TArray<SlotType> Slots;
		Slots.Reserve(SlotNames.Num());
		for (const FName SlotName : SlotNames)
		{
			const SlotType* Kept = Existing.FindByPredicate([SlotName](const SlotType& Slot) { return Slot.MaterialSlotName == SlotName; });
			SlotType& Slot = Slots.Emplace_GetRef(Kept ? Kept->MaterialInterface.Get() : UMaterial::GetDefaultMaterial(MD_Surface), SlotName, SlotName);
			Slot.UVChannelData = FMeshUVChannelInfo(1.0f);
		}
		return Slots;
	}

	/** Names each polygon group's material slot: the mesh converter makes polygon group N for material ID N, and each triangle's material ID is its slot index. */
	inline void NamePolygonGroupSlots(FMeshDescription& Description, TConstArrayView<FName> SlotNames)
	{
		TPolygonGroupAttributesRef<FName> GroupSlotNames =
			Description.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
		for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
		{
			if (SlotNames.IsValidIndex(Group.GetValue()))
			{
				GroupSlotNames[Group] = SlotNames[Group.GetValue()];
			}
		}
	}
}
