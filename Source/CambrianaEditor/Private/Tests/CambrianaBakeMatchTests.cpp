#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "Components/DynamicMeshComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	/** ATS-46: bakes the fixture grammar and places a live actor from it; the baked mesh's rendered triangles must match the actor's. */
	bool TestBakedMatchesPlacedActor(FAutomationTestBase& Test, const TCHAR* AssetName, const TCHAR* Label, const FString& GrammarText)
	{
		FBakeFixture Fixture(AssetName);
		Fixture.Grammar->GrammarText = GrammarText;
		Fixture.Grammar->Seed = 7177;
		if (!Test.TestTrue(FString::Printf(TEXT("%s bakes"), Label), Fixture.Bake().Status == ECambrianaBakeStatus::Baked))
		{
			return false;
		}
		UStaticMesh* Mesh = Fixture.FindMesh();
		ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
		if (!Test.TestNotNull(TEXT("The baked mesh exists"), Mesh) || !Test.TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
		{
			DestroyActor(Actor);
			return false;
		}

		const FTriangleMatch Match = MatchTriangles(*Actor->GetDynamicMeshComponent()->GetMesh(), *Mesh);
		Test.TestTrue(FString::Printf(TEXT("%s: the baked mesh's rendered triangles and vertices match the live actor's within 0.001 cm (%s)"),
			Label, *Match.Describe()), Match.Matches());
		DestroyActor(Actor);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeMatchesLiveActorTest,
	"Cambriana.Editor.Bake.BakedStaticMeshVerticesMatchTheLiveActorBuiltFromTheGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeMatchesLiveActorTest::RunTest(const FString& Parameters)
{
	TestBakedMatchesPlacedActor(*this, TEXT("BakeMatchSk"), TEXT("sk.ls"), LoadFile(FixturePath(TEXT("sk.ls"))));
	TestBakedMatchesPlacedActor(*this, TEXT("BakeMatchSkLsys"), TEXT("sk.lsys"), LoadFile(FixturePath(TEXT("sk.lsys"))));
	TestBakedMatchesPlacedActor(*this, TEXT("BakeMatchTree11"), TEXT("TREE11.LS"), LoadFile(FixturePath(TEXT("TREE11.LS"))));
	TestBakedMatchesPlacedActor(*this, TEXT("BakeMatchZeroThickness"), TEXT("zero thickness"), TEXT("1\n25\n50\nF!(0)F\n@\n"));
	// About 0.00002 cm between neighbouring ring corners at the base: within the static mesh build's THRESH_POINTS_ARE_SAME
	// (0.00002) of each other, yet distinct points in single precision near the origin.
	TestBakedMatchesPlacedActor(*this, TEXT("BakeMatchNearZeroThickness"), TEXT("near-zero thickness"), TEXT("1\n25\n50\n!(0.0000005)F\n@\n"));
	return true;
}

#endif
