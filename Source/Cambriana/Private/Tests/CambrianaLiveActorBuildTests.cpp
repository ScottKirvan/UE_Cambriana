#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;
using Cambriana::Tests::LoadFixture;

namespace
{
	constexpr double EditorFrameSeconds = 1.0 / 60.0;

	ACambrianaLiveActor* SpawnFixtureActor(UWorld* World, const TCHAR* Fixture)
	{
		ACambrianaLiveActor* Actor = World->SpawnActor<ACambrianaLiveActor>();
		if (Actor)
		{
			Actor->InlineGrammar = LoadFixture(Fixture);
			Actor->Seed = 12345;
			Actor->Rebuild();
		}
		return Actor;
	}

	const FDynamicMesh3& MeshOf(const ACambrianaLiveActor* Actor)
	{
		return *Actor->GetDynamicMeshComponent()->GetMesh();
	}

	TArray<FVector3d> PositionsOf(const ACambrianaLiveActor* Actor)
	{
		TArray<FVector3d> Positions;
		const FDynamicMesh3& Mesh = MeshOf(Actor);
		for (const int32 Vid : Mesh.VertexIndicesItr())
		{
			Positions.Add(Mesh.GetVertex(Vid));
		}
		return Positions;
	}

	TArray<float> BranchRandomsOf(const ACambrianaLiveActor* Actor)
	{
		TArray<float> Values;
		const FDynamicMesh3& Mesh = MeshOf(Actor);
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorRecursionMappingTest,
	"Cambriana.LiveActor.Mapping.RecursionOverride",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorRecursionMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	TestEqual(TEXT("Without the override the grammar's recursion is used"), Actor->GetLastBuildReport().Recursion, 20.0);
	TestEqual(TEXT("sk.ls at recursion 20"), MeshOf(Actor).VertexCount(), 672);

	Actor->bOverrideRecursion = true;
	Actor->Recursion = 5.0;
	Actor->Rebuild();
	TestEqual(TEXT("The override is used"), Actor->GetLastBuildReport().Recursion, 5.0);
	TestEqual(TEXT("Four chains of five segments"), MeshOf(Actor).VertexCount(), 8 * (4 * 5 + 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBasicAngleMappingTest,
	"Cambriana.LiveActor.Mapping.BasicAngleOverride",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBasicAngleMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	TestEqual(TEXT("Without the override the grammar's angle is used"), Actor->GetLastBuildReport().BasicAngle, 25.0);
	const TArray<FVector3d> Before = PositionsOf(Actor);

	Actor->bOverrideBasicAngle = true;
	Actor->BasicAngle = 10.0;
	Actor->Rebuild();
	TestEqual(TEXT("The override is used"), Actor->GetLastBuildReport().BasicAngle, 10.0);
	TestTrue(TEXT("The shape changes"), PositionsOf(Actor) != Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorThicknessMappingTest,
	"Cambriana.LiveActor.Mapping.ThicknessOverride",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorThicknessMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	TestEqual(TEXT("Without the override the grammar's thickness is used"), Actor->GetLastBuildReport().Thickness, 50.0);
	const TArray<FVector3d> Before = PositionsOf(Actor);

	Actor->bOverrideThickness = true;
	Actor->Thickness = 20.0;
	Actor->Rebuild();
	TestEqual(TEXT("The override is used"), Actor->GetLastBuildReport().Thickness, 20.0);
	TestTrue(TEXT("The shape changes"), PositionsOf(Actor) != Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorMinThicknessMappingTest,
	"Cambriana.LiveActor.Mapping.MinThickness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorMinThicknessMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	Actor->SizeMode = ECambrianaSizeMode::Step;
	Actor->SizeValue = 10.0;
	Actor->Rebuild();
	const FVector3d ThinExtents = MeshOf(Actor).GetBounds().Extents();

	Actor->MinThickness = 20.0;
	Actor->Rebuild();
	const FVector3d ThickExtents = MeshOf(Actor).GetBounds().Extents();
	TestTrue(TEXT("A minimum thickness above the grammar's widens the tubes"),
		ThickExtents.X > ThinExtents.X && ThickExtents.Y > ThinExtents.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorClosedFormMappingTest,
	"Cambriana.LiveActor.Mapping.ClosedForm",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorClosedFormMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	TestEqual(TEXT("Closed form builds tubes"), MeshOf(Actor).VertexCount(), 672);

	Actor->bClosedForm = false;
	Actor->Rebuild();
	TestTrue(TEXT("Open form builds geometry"), MeshOf(Actor).TriangleCount() > 0);
	TestNotEqual(TEXT("Open form builds blocks, not tubes"), MeshOf(Actor).VertexCount(), 672);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSidesMappingTest,
	"Cambriana.LiveActor.Mapping.Sides",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSidesMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	for (const int32 Sides : { 3, 5, 12 })
	{
		Actor->Sides = Sides;
		Actor->Rebuild();
		TestEqual(FString::Printf(TEXT("84 rings of %d vertices"), Sides), MeshOf(Actor).VertexCount(), 84 * Sides);
		TestEqual(FString::Printf(TEXT("80 segments of %d quads"), Sides), MeshOf(Actor).TriangleCount(), 80 * 2 * Sides);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSizeMappingTest,
	"Cambriana.LiveActor.Mapping.SizeModeAndValue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSizeMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	TestNearlyEqual(TEXT("Fit 200 cm by default"), MeshOf(Actor).GetBounds().MaxDim(), 200.0, 1e-6);

	Actor->SizeValue = 300.0;
	Actor->Rebuild();
	TestNearlyEqual(TEXT("Fit to the size value"), MeshOf(Actor).GetBounds().MaxDim(), 300.0, 1e-6);

	Actor->SizeMode = ECambrianaSizeMode::Step;
	Actor->SizeValue = 7.0;
	Actor->Rebuild();
	TestEqual(TEXT("Step mode uses the size value as the step length"), Actor->GetLastBuildReport().StepLength, 7.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSeedMappingTest,
	"Cambriana.LiveActor.Mapping.Seed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSeedMappingTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	Actor->Seed = 1;
	Actor->Rebuild();
	const TArray<float> First = BranchRandomsOf(Actor);
	TestTrue(TEXT("Branch randoms present"), First.Num() > 0);
	Actor->Rebuild();
	TestTrue(TEXT("The same seed gives the same branch randoms"), BranchRandomsOf(Actor) == First);

	Actor->Seed = 2;
	Actor->Rebuild();
	TestTrue(TEXT("Another seed gives other branch randoms"), BranchRandomsOf(Actor) != First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorErrorGrammarTest,
	"Cambriana.LiveActor.GrammarWithErrorsLeavesAnEmptyMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorErrorGrammarTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("Grammar error at line"), ELogVerbosity::Error);
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	TestTrue(TEXT("Valid grammar builds geometry"), MeshOf(Actor).TriangleCount() > 0);

	Actor->InlineGrammar = TEXT("20\n25\n50\n");
	Actor->Rebuild();
	TestEqual(TEXT("No vertices"), MeshOf(Actor).VertexCount(), 0);
	TestEqual(TEXT("No triangles"), MeshOf(Actor).TriangleCount(), 0);
	TestTrue(TEXT("The actor keeps the errors"), Actor->GetLastBuildReport().bHasErrors);
	TestTrue(TEXT("The actor keeps the diagnostics"), Actor->GetLastBuildReport().Diagnostics.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorWorkBudgetTest,
	"Cambriana.LiveActor.DefaultWorkBudgetBoundsAHugeRecursion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorWorkBudgetTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("work budget reached"), ELogVerbosity::Warning);
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	Actor->bOverrideRecursion = true;
	Actor->Recursion = 100000.0;
	Actor->Rebuild();
	TestTrue(TEXT("The actor keeps the budget flag"), Actor->GetLastBuildReport().bWorkBudgetReached);
	TestTrue(TEXT("Rewriting stopped early"), Actor->GetLastBuildReport().GenerationsRun < 100000);
	TestTrue(TEXT("The last complete generation is shown"), MeshOf(Actor).TriangleCount() > 0);
	return true;
}

#if WITH_EDITOR

using Cambriana::Tests::EditProperty;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSkRebuildFrameTest,
	"Cambriana.LiveActor.Performance.SkLsBasicAngleRebuildWithinOneEditorFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSkRebuildFrameTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("sk.ls"));
	double Slowest = 0.0;
	for (const double Angle : { 5.0, 15.0, 25.0, 33.0, 40.0 })
	{
		const int32 Builds = Actor->GetBuildCount();
		const double Start = FPlatformTime::Seconds();
		EditProperty(Actor, GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle),
			[Actor, Angle] { Actor->bOverrideBasicAngle = true; Actor->BasicAngle = Angle; });
		const double Elapsed = FPlatformTime::Seconds() - Start;
		Slowest = FMath::Max(Slowest, Elapsed);
		TestEqual(FString::Printf(TEXT("Angle %g rebuilt"), Angle), Actor->GetBuildCount(), Builds + 1);
		TestTrue(FString::Printf(TEXT("Angle %g rebuilt in %.3f ms, within one %.3f ms editor frame"), Angle, Elapsed * 1000.0, EditorFrameSeconds * 1000.0),
			Elapsed < EditorFrameSeconds);
	}
	TestTrue(TEXT("sk.ls built geometry"), MeshOf(Actor).TriangleCount() > 0);
	AddInfo(FString::Printf(TEXT("sk.ls Basic Angle rebuild: slowest %.3f ms"), Slowest * 1000.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorTreeRebuildTest,
	"Cambriana.LiveActor.Performance.Tree11Recursion12RebuildUnderOneSecond",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorTreeRebuildTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnFixtureActor(World.Get(), TEXT("TREE11.LS"));
	Actor->bOverrideRecursion = true;
	Actor->Recursion = 12.0;
	Actor->Rebuild();
	TestEqual(TEXT("Recursion 12"), Actor->GetLastBuildReport().Recursion, 12.0);
	TestEqual(TEXT("TREE11.LS production length at recursion 12"), Actor->GetLastBuildReport().ProductionLength, uint64(529263));

	double Slowest = 0.0;
	const auto ExpectFastRebuild = [this, Actor, &Slowest](FName PropertyName, TFunctionRef<void()> Change)
	{
		const int32 Builds = Actor->GetBuildCount();
		const double Start = FPlatformTime::Seconds();
		EditProperty(Actor, PropertyName, Change);
		const double Elapsed = FPlatformTime::Seconds() - Start;
		Slowest = FMath::Max(Slowest, Elapsed);
		TestEqual(FString::Printf(TEXT("Changing %s rebuilt"), *PropertyName.ToString()), Actor->GetBuildCount(), Builds + 1);
		TestTrue(FString::Printf(TEXT("Changing %s rebuilt in %.1f ms, under 1 s"), *PropertyName.ToString(), Elapsed * 1000.0), Elapsed < 1.0);
		TestTrue(FString::Printf(TEXT("After %s the mesh has geometry"), *PropertyName.ToString()), MeshOf(Actor).TriangleCount() > 0);
	};

	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle), [Actor] { Actor->bOverrideBasicAngle = true; Actor->BasicAngle = 20.0; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Thickness), [Actor] { Actor->bOverrideThickness = true; Actor->Thickness = 40.0; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, MinThickness), [Actor] { Actor->MinThickness = 0.2; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Seed), [Actor] { Actor->Seed += 1; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides), [Actor] { Actor->Sides = 12; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm), [Actor] { Actor->bClosedForm = false; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm), [Actor] { Actor->bClosedForm = true; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue), [Actor] { Actor->SizeValue = 400.0; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode), [Actor] { Actor->SizeMode = ECambrianaSizeMode::Step; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue), [Actor] { Actor->SizeValue = 3.0; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Recursion), [Actor] { Actor->Recursion = 11.0; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Recursion), [Actor] { Actor->Recursion = 12.0; });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, InlineGrammar), [Actor] { Actor->InlineGrammar = LoadFixture(TEXT("TREE11.LS")) + TEXT("\n"); });
	ExpectFastRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Grammar), [Actor]
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
		Grammar->GrammarText = LoadFixture(TEXT("TREE11.LS"));
		Actor->Grammar = Grammar;
	});
	AddInfo(FString::Printf(TEXT("TREE11.LS recursion 12 rebuild: slowest %.1f ms"), Slowest * 1000.0));
	return true;
}

#endif

#endif
