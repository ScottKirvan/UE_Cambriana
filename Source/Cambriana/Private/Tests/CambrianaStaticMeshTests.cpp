#include "CambrianaBuild.h"
#include "CambrianaStaticMesh.h"
#include "CambrianaTestUtilities.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::LoadFixture;

namespace
{
	// sk.ls draws its four branches after `c` once, twice, three and four times from colour 0, and draws nothing outside them
	// (FMT-legacy-ls: `c` adds 1 to the colour, `[` pushes it), so its material IDs are 1 to 4.
	const TArray<int64> SkMaterialIds = { 1, 2, 3, 4 };

	// SDD-surfacing.md, Tubes: sk.ls at 8 sides has 1,280 triangles.
	constexpr int32 SkTriangles = 1280;

	FCambrianaBuildResult BuildSk()
	{
		FCambrianaBuildInput Input;
		Input.GrammarText = LoadFixture(TEXT("sk.ls"));
		Input.Seed = 12345;
		return Cambriana::BuildMesh(Input);
	}

	int32 RenderedTriangles(UStaticMesh& Mesh)
	{
		const FStaticMeshRenderData* RenderData = Mesh.GetRenderData();
		return RenderData && RenderData->LODResources.Num() > 0 ? RenderData->LODResources[0].GetNumTriangles() : 0;
	}

	int32 RenderedSections(UStaticMesh& Mesh)
	{
		const FStaticMeshRenderData* RenderData = Mesh.GetRenderData();
		return RenderData && RenderData->LODResources.Num() > 0 ? RenderData->LODResources[0].Sections.Num() : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaStaticMeshSlotNameTest,
	"Cambriana.StaticMesh.MaterialSlotIsNamedAfterItsMaterialId",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaStaticMeshSlotNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Material ID 0"), Cambriana::MaterialSlotName(0), FName(TEXT("Material_0")));
	TestEqual(TEXT("Material ID 12"), Cambriana::MaterialSlotName(12), FName(TEXT("Material_12")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaStaticMeshFromBuildTest,
	"Cambriana.StaticMesh.BuildResultMakesTheMeshWithOneDefaultMaterialSlotPerMaterialId",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaStaticMeshFromBuildTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = BuildSk();
	if (!TestTrue(TEXT("sk.ls builds its four material IDs"), Result.SlotMaterialIds == SkMaterialIds))
	{
		return false;
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	Cambriana::SetStaticMeshFromBuild(*Mesh, Result);

	TestEqual(TEXT("Triangles"), RenderedTriangles(*Mesh), SkTriangles);
	TestEqual(TEXT("One section per material ID"), RenderedSections(*Mesh), SkMaterialIds.Num());
	const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
	if (TestEqual(TEXT("One material slot per material ID"), Slots.Num(), SkMaterialIds.Num()))
	{
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const FName Expected(*FString::Printf(TEXT("Material_%lld"), SkMaterialIds[Index]));
			TestEqual(FString::Printf(TEXT("Slot %d is named after material ID %lld"), Index, SkMaterialIds[Index]), Slots[Index].MaterialSlotName, Expected);
			TestTrue(FString::Printf(TEXT("Slot %d holds the default surface material"), Index),
				Slots[Index].MaterialInterface == UMaterial::GetDefaultMaterial(MD_Surface));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaStaticMeshKeepsSlotMaterialTest,
	"Cambriana.StaticMesh.RebuildKeepsTheMaterialOfASlotWithTheSameName",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaStaticMeshKeepsSlotMaterialTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = BuildSk();
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	UMaterial* Assigned = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FStaticMaterial> Existing;
	Existing.Emplace(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("Material_9"), TEXT("Material_9"));
	Existing.Emplace(Assigned, TEXT("Material_3"), TEXT("Material_3"));
	Mesh->SetStaticMaterials(Existing);

	Cambriana::SetStaticMeshFromBuild(*Mesh, Result);

	const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
	if (TestEqual(TEXT("The slots are the build's material IDs, the old slot without one gone"), Slots.Num(), SkMaterialIds.Num()))
	{
		// Material ID 3 is the third of 1 to 4.
		TestEqual(TEXT("The third slot is Material_3"), Slots[2].MaterialSlotName, FName(TEXT("Material_3")));
		TestTrue(TEXT("Material_3 keeps its assigned material"), Slots[2].MaterialInterface == Assigned);
		TestTrue(TEXT("Material_1 gets the default surface material"), Slots[0].MaterialInterface == UMaterial::GetDefaultMaterial(MD_Surface));
	}
	TestEqual(TEXT("Triangles"), RenderedTriangles(*Mesh), SkTriangles);
	return true;
}

#endif
