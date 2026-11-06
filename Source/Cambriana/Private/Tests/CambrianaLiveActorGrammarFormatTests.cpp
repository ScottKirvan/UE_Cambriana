#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FIndex3i;
using Cambriana::Tests::LoadFixture;

namespace
{
	enum class ECoreEntry
	{
		Legacy,
		Grammar,
	};

	class FCoreOutput
	{
	public:
		FCoreOutput(ECoreEntry Entry, const FString& Text, uint32 Seed)
		{
			cambriana_build_settings Settings;
			cambriana_build_settings_init(&Settings, sizeof(Settings));
			Settings.seed = Seed;
			const FTCHARToUTF8 Utf8(*Text);
			if (Entry == ECoreEntry::Legacy)
			{
				cambriana_build_legacy(Utf8.Get(), Utf8.Length(), &Settings, nullptr, &Result);
			}
			else
			{
				Settings.overrides |= CAMBRIANA_OVERRIDE_SEED | CAMBRIANA_OVERRIDE_SIZE;
				cambriana_build_grammar(Utf8.Get(), Utf8.Length(), nullptr, &Settings, nullptr, &Result);
			}
		}

		~FCoreOutput() { cambriana_result_destroy(Result); }

		FCoreOutput(const FCoreOutput&) = delete;
		FCoreOutput& operator=(const FCoreOutput&) = delete;

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

	private:
		cambriana_result* Result = nullptr;
	};

	ACambrianaLiveActor* SpawnTextActor(UWorld* World, const FString& Text, uint32 Seed)
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

	const FDynamicMesh3& ActorMeshOf(const ACambrianaLiveActor* Actor)
	{
		return *Actor->GetDynamicMeshComponent()->GetMesh();
	}

	TArray<float> ActorBranchRandoms(const ACambrianaLiveActor* Actor)
	{
		TArray<float> Values;
		const FDynamicMesh3& Mesh = ActorMeshOf(Actor);
		if (!Mesh.HasAttributes() || Mesh.Attributes()->NumUVLayers() < 3)
		{
			return Values;
		}
		const UE::Geometry::FDynamicMeshUVOverlay* Uv2 = Mesh.Attributes()->GetUVLayer(2);
		for (const int32 Element : Uv2->ElementIndicesItr())
		{
			Values.Add(Uv2->GetElement(Element).Y);
		}
		return Values;
	}

	void ExpectMeshEqualsCore(FAutomationTestBase& Test, const FString& Label, const FDynamicMesh3& Mesh, const FCoreOutput& Core)
	{
		const TConstArrayView<cambriana_vec3> Positions = Core.Positions();
		const TConstArrayView<cambriana_triangle> Triangles = Core.Triangles();
		Test.TestTrue(Label + TEXT(": core produced geometry"), Triangles.Num() > 0);
		if (!Test.TestTrue(Label + TEXT(": every core vertex is present"), Mesh.VertexCount() >= Positions.Num())
			|| !Test.TestEqual(Label + TEXT(": triangle count"), Mesh.TriangleCount(), Triangles.Num()))
		{
			return;
		}

		int32 PositionMismatches = 0;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const cambriana_vec3& P = Positions[Index];
			PositionMismatches += Mesh.GetVertex(Index) != FVector3d(P.x, P.y, P.z);
		}
		Test.TestEqual(Label + TEXT(": vertex positions equal the core's"), PositionMismatches, 0);

		int32 CornerMismatches = 0;
		for (int32 Index = 0; Index < Triangles.Num(); ++Index)
		{
			const FIndex3i Triangle = Mesh.GetTriangle(Index);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const cambriana_vec3& P = Positions[static_cast<int32>(Triangles[Index].vertices[Corner])];
				CornerMismatches += Mesh.GetVertex(Triangle[Corner]) != FVector3d(P.x, P.y, P.z);
			}
		}
		Test.TestEqual(Label + TEXT(": triangle corners and winding equal the core's"), CornerMismatches, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorLsysMatchesCoreTest,
	"Cambriana.LiveActor.GrammarFormat.SkLsysEqualsCoreGrammarBuild",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorLsysMatchesCoreTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}
	const FCoreOutput Core(ECoreEntry::Grammar, Text, 4321);
	const ACambrianaLiveActor* Actor = SpawnTextActor(World.Get(), Text, 4321);
	if (!TestTrue(TEXT("Core built"), Core.IsValid()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	TestFalse(TEXT("No errors"), Actor->GetLastBuildReport().bHasErrors);
	ExpectMeshEqualsCore(*this, TEXT("sk.lsys"), ActorMeshOf(Actor), Core);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorLegacyUnchangedTest,
	"Cambriana.LiveActor.GrammarFormat.SkLsStillBuildsAsLegacy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorLegacyUnchangedTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FString Text = LoadFixture(TEXT("sk.ls"));
	const FCoreOutput Core(ECoreEntry::Legacy, Text, 4321);
	const ACambrianaLiveActor* Actor = SpawnTextActor(World.Get(), Text, 4321);
	if (!TestTrue(TEXT("Core built"), Core.IsValid()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	TestFalse(TEXT("No errors"), Actor->GetLastBuildReport().bHasErrors);
	TestEqual(TEXT("The legacy header's recursion is read"), Actor->GetLastBuildReport().Recursion, 20.0);
	TestEqual(TEXT("Vertices"), ActorMeshOf(Actor).VertexCount(), 672);
	TestEqual(TEXT("Triangles"), ActorMeshOf(Actor).TriangleCount(), 1280);
	ExpectMeshEqualsCore(*this, TEXT("sk.ls"), ActorMeshOf(Actor), Core);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorLsysErrorsTest,
	"Cambriana.LiveActor.GrammarFormat.LsysWithErrorsLeavesAnEmptyMeshAndLogsLineAndColumn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorLsysErrorsTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("Grammar error at line 3, column 3: syntax error"), ELogVerbosity::Error);
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnTextActor(World.Get(), LoadFixture(TEXT("sk.lsys")), 1);
	TestTrue(TEXT("Valid grammar builds geometry"), ActorMeshOf(Actor).TriangleCount() > 0);

	Actor->InlineGrammar = TEXT("axiom X\ngrow {\nX > F\n}\n");
	Actor->Rebuild();
	TestEqual(TEXT("No vertices"), ActorMeshOf(Actor).VertexCount(), 0);
	TestEqual(TEXT("No triangles"), ActorMeshOf(Actor).TriangleCount(), 0);
	const FCambrianaBuildReport& Report = Actor->GetLastBuildReport();
	TestTrue(TEXT("The actor keeps the errors"), Report.bHasErrors);
	const FCambrianaDiagnostic* Syntax = Report.Diagnostics.FindByPredicate([](const FCambrianaDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error && Diagnostic.Line == 3 && Diagnostic.Column == 3;
	});
	TestNotNull(TEXT("The actor keeps the error at line 3, column 3"), Syntax);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorLsysOwnSeedAndSizeTest,
	"Cambriana.LiveActor.GrammarFormat.ActorSeedAndSizeApplyOverTheGrammars",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorLsysOwnSeedAndSizeTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FString Text = TEXT("seed 7\nsize 50cm\n") + LoadFixture(TEXT("sk.lsys"));
	ACambrianaLiveActor* Actor = SpawnTextActor(World.Get(), Text, 1);
	TestFalse(TEXT("No errors"), Actor->GetLastBuildReport().bHasErrors);
	TestNearlyEqual(TEXT("The actor's Fit size applies, not the grammar's"), ActorMeshOf(Actor).GetBounds().MaxDim(), 200.0, 1e-6);
	const TArray<float> First = ActorBranchRandoms(Actor);
	TestTrue(TEXT("Branch randoms present"), First.Num() > 0);

	Actor->Seed = 2;
	Actor->Rebuild();
	TestTrue(TEXT("The actor's seed applies, not the grammar's"), ActorBranchRandoms(Actor) != First);
	return true;
}

#endif
