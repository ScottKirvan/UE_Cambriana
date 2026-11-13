#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
struct FCambrianaBuildResult;

namespace Cambriana
{
	/** The name of the material slot holding a core material ID's triangles: Material_<ID>. */
	CAMBRIANA_API FName MaterialSlotName(int64 MaterialId);

	/**
	 * Makes StaticMesh a single LOD holding the build result's mesh, with one material slot per material ID in the result's slot
	 * order. A slot keeps the material of the static mesh's existing slot of the same name; any other slot gets the engine's
	 * default surface material.
	 */
	CAMBRIANA_API void SetStaticMeshFromBuild(UStaticMesh& StaticMesh, const FCambrianaBuildResult& Result);
}
