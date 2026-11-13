#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaMeshComparison.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshResources.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// Every data channel varies: branches start chains (along-branch), the generations differ, branch random differs per chain,
	// thickness shrinks with `!`, and four attributes are set, so the colour's G, B and A take the first three in first-use
	// order (wet, age, heat) and `extra` is in no slot. The text states its size and sides, so the core's own defaults agree
	// with the bake's settings.
	const TCHAR* const ChannelGrammar = TEXT(
		"generations 3\n"
		"angle 30\n"
		"thickness 40%\n"
		"size 150\n"
		"sides 6\n"
		"axiom attr(wet, 0.25) attr(age, 0.5) attr(heat, 0.75) attr(extra, 0.9) A\n"
		"\n"
		"grow {\n"
		"    A -> F [ + attr(wet, 1) ! F ] [ - attr(heat, 0.2) attr(age, 0.1) F ] A\n"
		"}\n");

	constexpr uint32 ChannelSeed = 4242;

	/** One surface vertex's data channels, as SDD-surfacing.md (Data channels) maps them to vertex slots. */
	struct FExpectedVertex
	{
		FVector3d Position;
		FVector2f Uv1;
		FVector2f Uv2;
		FLinearColor Colour;
	};

	/**
	 * The oracle: the core's own channel output for the same build, read straight from its C API, then put in the slots the
	 * spec names. It is independent of the code under test, which is the plugin's mapping into the dynamic mesh, the conversion
	 * to the static mesh and the engine's build.
	 */
	TArray<FExpectedVertex> ExpectedVertices(const FString& Text, uint32 Seed)
	{
		cambriana_build_settings Settings;
		cambriana_build_settings_init(&Settings, sizeof(Settings));
		Settings.seed = Seed;
		Settings.overrides |= CAMBRIANA_OVERRIDE_SEED;
		const FTCHARToUTF8 Utf8(*Text);
		cambriana_result* Result = nullptr;
		cambriana_build_grammar(Utf8.Get(), Utf8.Length(), nullptr, &Settings, nullptr, &Result);
		TArray<FExpectedVertex> Expected;
		if (!Result)
		{
			return Expected;
		}

		uint64_t PositionCount = 0, ChannelCount = 0, NameCount = 0, AttributeCount = 0;
		const cambriana_vec3* Positions = cambriana_result_surface_positions(Result, &PositionCount);
		const cambriana_vertex_channels* Channels = cambriana_result_vertex_channels(Result, &ChannelCount);
		cambriana_result_attribute_names(Result, &NameCount);
		const double* Attributes = cambriana_result_vertex_attributes(Result, &AttributeCount);

		double MaxGeneration = 0.0, MaxThickness = 0.0;
		for (uint64_t Index = 0; Index < ChannelCount; ++Index)
		{
			MaxGeneration = FMath::Max(MaxGeneration, double(Channels[Index].generation));
			MaxThickness = FMath::Max(MaxThickness, Channels[Index].thickness);
		}
		auto Attribute = [&](uint64_t Vertex, uint64_t Column)
		{
			const uint64_t Index = Vertex * NameCount + Column;
			return Column < NameCount && Index < AttributeCount ? Attributes[Index] : 0.0;
		};

		for (uint64_t Index = 0; Index < FMath::Min(PositionCount, ChannelCount); ++Index)
		{
			const cambriana_vertex_channels& Channel = Channels[Index];
			FExpectedVertex& Vertex = Expected.AddDefaulted_GetRef();
			Vertex.Position = FVector3d(Positions[Index].x, Positions[Index].y, Positions[Index].z);
			Vertex.Uv1 = FVector2f(float(Channel.along_branch), float(MaxGeneration > 0.0 ? double(Channel.generation) / MaxGeneration : 0.0));
			Vertex.Uv2 = FVector2f(float(Channel.root_distance), float(Channel.branch_random));
			Vertex.Colour = FLinearColor(float(MaxThickness > 0.0 ? Channel.thickness / MaxThickness : 0.0), float(Attribute(Index, 0)),
				float(Attribute(Index, 1)), float(Attribute(Index, 2)));
		}
		cambriana_result_destroy(Result);
		return Expected;
	}

	/** One rendered vertex of LOD 0: its position, UV1, UV2 and the vertex colour a material's Vertex Color node reads (byte / 255). */
	struct FRenderedVertex
	{
		FVector3d Position;
		FVector2f Uv1;
		FVector2f Uv2;
		FLinearColor Colour;
	};

	TArray<FRenderedVertex> RenderedVertices(const UStaticMesh& Mesh)
	{
		TArray<FRenderedVertex> Vertices;
		const FStaticMeshRenderData* RenderData = Mesh.GetRenderData();
		if (!RenderData || RenderData->LODResources.IsEmpty())
		{
			return Vertices;
		}
		const FStaticMeshVertexBuffers& Buffers = RenderData->LODResources[0].VertexBuffers;
		if (Buffers.StaticMeshVertexBuffer.GetNumTexCoords() < 3 || Buffers.ColorVertexBuffer.GetNumVertices() != Buffers.PositionVertexBuffer.GetNumVertices())
		{
			return Vertices;
		}
		for (uint32 Index = 0; Index < Buffers.PositionVertexBuffer.GetNumVertices(); ++Index)
		{
			const FColor Byte = Buffers.ColorVertexBuffer.VertexColor(Index);
			Vertices.Add({ FVector3d(Buffers.PositionVertexBuffer.VertexPosition(Index)), Buffers.StaticMeshVertexBuffer.GetVertexUV(Index, 1),
				Buffers.StaticMeshVertexBuffer.GetVertexUV(Index, 2), FLinearColor(Byte.R / 255.0f, Byte.G / 255.0f, Byte.B / 255.0f, Byte.A / 255.0f) });
		}
		return Vertices;
	}

	bool SameChannels(const FExpectedVertex& Expected, const FRenderedVertex& Rendered)
	{
		// Ratios and random values to 1e-4; root distance, in cm, to the 0.001 cm of a vertex position; an 8-bit colour channel
		// to one step, its value clamped to [0, 1] as the byte stores it.
		constexpr float RatioTolerance = 1e-4f;
		constexpr float DistanceTolerance = 1e-3f;
		constexpr float ColourTolerance = 1.0f / 255.0f + 1e-6f;
		const FLinearColor Clamped = Expected.Colour.GetClamped();
		return FVector3d::Distance(Expected.Position, Rendered.Position) <= VertexTolerance
			&& FMath::IsNearlyEqual(Expected.Uv1.X, Rendered.Uv1.X, RatioTolerance)
			&& FMath::IsNearlyEqual(Expected.Uv1.Y, Rendered.Uv1.Y, RatioTolerance)
			&& FMath::IsNearlyEqual(Expected.Uv2.X, Rendered.Uv2.X, DistanceTolerance)
			&& FMath::IsNearlyEqual(Expected.Uv2.Y, Rendered.Uv2.Y, RatioTolerance)
			&& FMath::IsNearlyEqual(Clamped.R, Rendered.Colour.R, ColourTolerance)
			&& FMath::IsNearlyEqual(Clamped.G, Rendered.Colour.G, ColourTolerance)
			&& FMath::IsNearlyEqual(Clamped.B, Rendered.Colour.B, ColourTolerance)
			&& FMath::IsNearlyEqual(Clamped.A, Rendered.Colour.A, ColourTolerance);
	}

	FString Describe(const FExpectedVertex& Vertex)
	{
		return FString::Printf(TEXT("at %s UV1 %s UV2 %s colour %s"), *Vertex.Position.ToString(), *Vertex.Uv1.ToString(), *Vertex.Uv2.ToString(),
			*Vertex.Colour.ToString());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeDataChannelsTest,
	"Cambriana.Editor.Bake.BakedStaticMeshHoldsTheDataChannelsInTheirVertexSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeDataChannelsTest::RunTest(const FString& Parameters)
{
	const TArray<FExpectedVertex> Expected = ExpectedVertices(ChannelGrammar, ChannelSeed);
	if (!TestTrue(TEXT("The core builds the channel grammar"), Expected.Num() > 0))
	{
		return false;
	}
	TestTrue(TEXT("The grammar sets every user attribute slot somewhere"), Expected.ContainsByPredicate([](const FExpectedVertex& Vertex)
		{ return Vertex.Colour.G > 0.0f && Vertex.Colour.B > 0.0f && Vertex.Colour.A > 0.0f; }));
	TestTrue(TEXT("Root distance goes past the range a half-precision UV holds to 0.001 cm"),
		Expected.ContainsByPredicate([](const FExpectedVertex& Vertex) { return Vertex.Uv2.X > 64.0f; }));

	FBakeFixture Fixture(TEXT("BakeChannels"));
	Fixture.Grammar->GrammarText = ChannelGrammar;
	Fixture.Grammar->Seed = ChannelSeed;
	if (!TestTrue(TEXT("The grammar bakes"), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	const TArray<FRenderedVertex> Rendered = RenderedVertices(*Fixture.FindMesh());
	if (!TestTrue(TEXT("The baked mesh renders vertices with UV1, UV2 and a vertex colour"), Rendered.Num() > 0))
	{
		return false;
	}

	FPointGrid RenderedPositions;
	for (int32 Index = 0; Index < Rendered.Num(); ++Index)
	{
		RenderedPositions.Add(Rendered[Index].Position, Index);
	}
	int32 Missing = 0;
	FString FirstMissing;
	for (const FExpectedVertex& Vertex : Expected)
	{
		if (RenderedPositions.FindNear(Vertex.Position, [&](int32 Index) { return SameChannels(Vertex, Rendered[Index]); }) == INDEX_NONE)
		{
			if (Missing++ == 0)
			{
				FirstMissing = Describe(Vertex);
			}
		}
	}
	TestEqual(FString::Printf(TEXT("Core vertices whose channels no rendered vertex holds in UV1 (along-branch, generation / max), UV2 (root distance cm, branch random) and colour (thickness / max, first three attributes) (of %d; first %s)"),
		Expected.Num(), *FirstMissing), Missing, 0);

	FPointGrid ExpectedPositions;
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		ExpectedPositions.Add(Expected[Index].Position, Index);
	}
	int32 Unexpected = 0;
	for (const FRenderedVertex& Vertex : Rendered)
	{
		Unexpected += ExpectedPositions.FindNear(Vertex.Position, [&](int32 Index) { return SameChannels(Expected[Index], Vertex); }) == INDEX_NONE;
	}
	TestEqual(FString::Printf(TEXT("Rendered vertices whose channels no core vertex has (of %d)"), Rendered.Num()), Unexpected, 0);
	return true;
}

#endif
