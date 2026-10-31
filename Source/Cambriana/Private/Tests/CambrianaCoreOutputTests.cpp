#include "CambrianaBuild.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FIndex3i;

namespace
{
	class FCoreBuild
	{
	public:
		FCoreBuild(const FString& Text, uint32 Seed)
		{
			cambriana_build_settings Settings;
			cambriana_build_settings_init(&Settings, sizeof(Settings));
			Settings.seed = Seed;
			const FTCHARToUTF8 Utf8(*Text);
			cambriana_build_legacy(Utf8.Get(), Utf8.Length(), &Settings, nullptr, &Result);
		}

		~FCoreBuild() { cambriana_result_destroy(Result); }

		FCoreBuild(const FCoreBuild&) = delete;
		FCoreBuild& operator=(const FCoreBuild&) = delete;

		bool IsValid() const { return Result != nullptr; }

		TConstArrayView<cambriana_vec3> Positions() const
		{
			uint64_t Count = 0;
			const cambriana_vec3* Data = cambriana_result_surface_positions(Result, &Count);
			return TConstArrayView<cambriana_vec3>(Data, static_cast<int32>(Count));
		}

		TConstArrayView<cambriana_triangle> Triangles() const
		{
			uint64_t Count = 0;
			const cambriana_triangle* Data = cambriana_result_triangles(Result, &Count);
			return TConstArrayView<cambriana_triangle>(Data, static_cast<int32>(Count));
		}

		TConstArrayView<cambriana_vertex_channels> Channels() const
		{
			uint64_t Count = 0;
			const cambriana_vertex_channels* Data = cambriana_result_vertex_channels(Result, &Count);
			return TConstArrayView<cambriana_vertex_channels>(Data, static_cast<int32>(Count));
		}

	private:
		cambriana_result* Result = nullptr;
	};

	FVector3d ToVector(const cambriana_vec3& V)
	{
		return FVector3d(V.x, V.y, V.z);
	}

	ACambrianaLiveActor* SpawnInlineActor(UWorld* World, const FString& Text, uint32 Seed)
	{
		ACambrianaLiveActor* Actor = World->SpawnActor<ACambrianaLiveActor>();
		if (Actor)
		{
			Actor->InlineGrammar = Text;
			Actor->Seed = Seed;
			Actor->Rebuild();
		}
		return Actor;
	}

	const FDynamicMesh3* ActorMesh(const ACambrianaLiveActor* Actor)
	{
		return Actor->GetDynamicMeshComponent()->GetMesh();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorMatchesCoreTest,
	"Cambriana.LiveActor.PositionsAndWindingEqualCoreOutput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorMatchesCoreTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	for (const TCHAR* Fixture : { TEXT("sk.ls"), TEXT("TREE11.LS") })
	{
		const FString Text = Cambriana::Tests::LoadFixture(Fixture);
		const FCoreBuild Core(Text, 4321);
		const ACambrianaLiveActor* Actor = SpawnInlineActor(World.Get(), Text, 4321);
		if (!TestTrue(FString::Printf(TEXT("%s: core built"), Fixture), Core.IsValid()) || !TestNotNull(TEXT("Actor spawned"), Actor))
		{
			return false;
		}
		const FDynamicMesh3& Mesh = *ActorMesh(Actor);
		const TConstArrayView<cambriana_vec3> Positions = Core.Positions();
		const TConstArrayView<cambriana_triangle> Triangles = Core.Triangles();
		TestTrue(FString::Printf(TEXT("%s: core produced geometry"), Fixture), Triangles.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s: every core vertex is present"), Fixture), Mesh.VertexCount() >= Positions.Num());
		if (!TestEqual(FString::Printf(TEXT("%s: triangle count"), Fixture), Mesh.TriangleCount(), Triangles.Num()))
		{
			continue;
		}

		int32 PositionMismatches = 0;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			PositionMismatches += Mesh.GetVertex(Index) != ToVector(Positions[Index]);
		}
		TestEqual(FString::Printf(TEXT("%s: vertex positions equal the core's"), Fixture), PositionMismatches, 0);

		int32 WindingMismatches = 0;
		for (int32 Index = 0; Index < Triangles.Num(); ++Index)
		{
			const FIndex3i Triangle = Mesh.GetTriangle(Index);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				WindingMismatches += Mesh.GetVertex(Triangle[Corner]) != ToVector(Positions[static_cast<int32>(Triangles[Index].vertices[Corner])]);
			}
		}
		TestEqual(FString::Printf(TEXT("%s: triangle corners and winding equal the core's"), Fixture), WindingMismatches, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorNormalsFaceOutTest,
	"Cambriana.LiveActor.NormalsFaceTheCoreFrontSide",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorNormalsFaceOutTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	for (const TCHAR* Fixture : { TEXT("sk.ls"), TEXT("TREE11.LS") })
	{
		const ACambrianaLiveActor* Actor = SpawnInlineActor(World.Get(), Cambriana::Tests::LoadFixture(Fixture), 1);
		const FDynamicMesh3& Mesh = *ActorMesh(Actor);
		if (!TestTrue(FString::Printf(TEXT("%s: has geometry"), Fixture), Mesh.TriangleCount() > 0)
			|| !TestTrue(TEXT("Attributes present"), Mesh.HasAttributes()))
		{
			return false;
		}
		const UE::Geometry::FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
		if (!TestNotNull(TEXT("Normals present"), Normals))
		{
			return false;
		}
		int32 Backward = 0;
		int32 Unset = 0;
		for (const int32 Tid : Mesh.TriangleIndicesItr())
		{
			FVector3d A, B, C;
			Mesh.GetTriVertices(Tid, A, B, C);
			const FVector3d Front = (C - A).Cross(B - A);
			if (Front.Length() < 1e-9)
			{
				continue;
			}
			if (!Normals->IsSetTriangle(Tid))
			{
				++Unset;
				continue;
			}
			FVector3f N0, N1, N2;
			Normals->GetTriElements(Tid, N0, N1, N2);
			Backward += FVector3d(N0 + N1 + N2).Dot(Front) <= 0.0;
		}
		TestEqual(FString::Printf(TEXT("%s: every triangle has normals"), Fixture), Unset, 0);
		TestEqual(FString::Printf(TEXT("%s: no normal faces away from the front side"), Fixture), Backward, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDataChannelsTest,
	"Cambriana.LiveActor.DataChannelsFillSpecifiedSlots",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDataChannelsTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FString Text = Cambriana::Tests::LoadFixture(TEXT("sk.ls"));
	const FCoreBuild Core(Text, 99);
	const ACambrianaLiveActor* Actor = SpawnInlineActor(World.Get(), Text, 99);
	const FDynamicMesh3& Mesh = *ActorMesh(Actor);
	const UE::Geometry::FDynamicMeshAttributeSet* Attributes = Mesh.HasAttributes() ? Mesh.Attributes() : nullptr;
	if (!TestNotNull(TEXT("Attributes present"), Attributes) || !TestEqual(TEXT("Three UV layers"), Attributes->NumUVLayers(), 3)
		|| !TestTrue(TEXT("Vertex colours present"), Attributes->HasPrimaryColors()))
	{
		return false;
	}

	const TConstArrayView<cambriana_vertex_channels> Channels = Core.Channels();
	uint64 MaxGeneration = 0;
	double MaxThickness = 0.0;
	for (const cambriana_vertex_channels& Channel : Channels)
	{
		MaxGeneration = FMath::Max<uint64>(MaxGeneration, Channel.generation);
		MaxThickness = FMath::Max(MaxThickness, Channel.thickness);
	}
	TestTrue(TEXT("sk.ls spans several generations"), MaxGeneration > 0);
	TestTrue(TEXT("sk.ls has thickness"), MaxThickness > 0.0);

	const UE::Geometry::FDynamicMeshUVOverlay* Uv1 = Attributes->GetUVLayer(1);
	const UE::Geometry::FDynamicMeshUVOverlay* Uv2 = Attributes->GetUVLayer(2);
	const UE::Geometry::FDynamicMeshColorOverlay* Colours = Attributes->PrimaryColors();
	const TConstArrayView<cambriana_triangle> Triangles = Core.Triangles();
	if (!TestEqual(TEXT("Triangle count"), Mesh.TriangleCount(), Triangles.Num()))
	{
		return false;
	}
	int32 Mismatches = 0;
	for (int32 Tid = 0; Tid < Triangles.Num(); ++Tid)
	{
		const FIndex3i Uv1Elements = Uv1->GetTriangle(Tid);
		const FIndex3i Uv2Elements = Uv2->GetTriangle(Tid);
		const FIndex3i ColourElements = Colours->GetTriangle(Tid);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const cambriana_vertex_channels& Channel = Channels[static_cast<int32>(Triangles[Tid].vertices[Corner])];
			const FVector2f ExpectedUv1(float(Channel.along_branch), float(double(Channel.generation) / double(MaxGeneration)));
			const FVector2f ExpectedUv2(float(Channel.root_distance), float(Channel.branch_random));
			const FVector4f ExpectedColour(float(Channel.thickness / MaxThickness), 0.0f, 0.0f, 0.0f);
			Mismatches += !Uv1->GetElement(Uv1Elements[Corner]).Equals(ExpectedUv1, 1e-6f);
			Mismatches += !Uv2->GetElement(Uv2Elements[Corner]).Equals(ExpectedUv2, 1e-4f);
			Mismatches += !FVector4f(Colours->GetElement(ColourElements[Corner])).Equals(ExpectedColour, 1e-6f);
		}
	}
	TestEqual(TEXT("UV1, UV2 and vertex colour hold the core's channels"), Mismatches, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorMaterialSlotsTest,
	"Cambriana.LiveActor.MaterialIdsBecomePreviewPaletteSlots",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorMaterialSlotsTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FString Text = Cambriana::Tests::LoadFixture(TEXT("sk.ls"));
	const FCoreBuild Core(Text, 5);
	const ACambrianaLiveActor* Actor = SpawnInlineActor(World.Get(), Text, 5);

	TArray<int64> CoreIds;
	for (const cambriana_triangle& Triangle : Core.Triangles())
	{
		CoreIds.AddUnique(Triangle.material);
	}
	CoreIds.Sort();
	TestEqual(TEXT("sk.ls uses four material IDs"), CoreIds.Num(), 4);

	const UDynamicMeshComponent* Component = Actor->GetDynamicMeshComponent();
	if (!TestEqual(TEXT("One material slot per material ID"), Component->GetNumMaterials(), CoreIds.Num()))
	{
		return false;
	}
	for (int32 Slot = 0; Slot < CoreIds.Num(); ++Slot)
	{
		const UMaterialInterface* Material = Component->GetMaterial(Slot);
		FLinearColor Colour;
		if (TestNotNull(FString::Printf(TEXT("Slot %d has a material"), Slot), Material)
			&& TestTrue(FString::Printf(TEXT("Slot %d material has a colour"), Slot), Material->GetVectorParameterValue(FName(TEXT("Color")), Colour)))
		{
			TestEqual(FString::Printf(TEXT("Slot %d colour is the palette colour of ID %lld"), Slot, CoreIds[Slot]),
				Colour, Cambriana::PreviewPaletteColor(CoreIds[Slot]));
		}
	}

	const FDynamicMesh3& Mesh = *ActorMesh(Actor);
	const UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
	if (!TestNotNull(TEXT("Material IDs present"), MaterialIds))
	{
		return false;
	}
	const TConstArrayView<cambriana_triangle> Triangles = Core.Triangles();
	if (!TestEqual(TEXT("Triangle count"), Mesh.TriangleCount(), Triangles.Num()))
	{
		return false;
	}
	int32 Mismatches = 0;
	for (int32 Tid = 0; Tid < Triangles.Num(); ++Tid)
	{
		const int32 Slot = MaterialIds->GetValue(Tid);
		Mismatches += !CoreIds.IsValidIndex(Slot) || CoreIds[Slot] != Triangles[Tid].material;
	}
	TestEqual(TEXT("Each triangle's slot holds its core material ID"), Mismatches, 0);
	return true;
}

#endif
