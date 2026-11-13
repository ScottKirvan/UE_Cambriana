#include "CambrianaStaticMesh.h"

#include "CambrianaBuild.h"
#include "Engine/StaticMesh.h"

FName Cambriana::MaterialSlotName(int64 MaterialId)
{
	return NAME_None;
}

void Cambriana::SetStaticMeshFromBuild(UStaticMesh& StaticMesh, const FCambrianaBuildResult& Result)
{
}
