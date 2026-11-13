#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CambrianaLiveActorPlacementTests
{
	const TCHAR* GrowRule = TEXT("axiom A\n\ngrow {\n    A -> F A\n}\n");

	FString Describe(const ACambrianaLiveActor& Actor)
	{
		return FString::Printf(TEXT("recursion %g, angle %g, thickness %g, size %s %g, sides %d, surface %s, min thickness %g"),
			Actor.Recursion, Actor.BasicAngle, Actor.Thickness, Actor.SizeMode == ECambrianaSizeMode::Fit ? TEXT("Fit") : TEXT("Step"),
			Actor.SizeValue, Actor.Sides, Actor.bClosedForm ? TEXT("tubes") : TEXT("blocks"), Actor.MinThickness);
	}

	FString DescribeOverrides(const ACambrianaLiveActor& Actor)
	{
		const bool Flags[] = { Actor.bOverrideRecursion, Actor.bOverrideBasicAngle, Actor.bOverrideThickness, Actor.bOverrideSize,
			Actor.bOverrideSides, Actor.bOverrideClosedForm, Actor.bOverrideMinThickness };
		FString Text;
		for (const bool bFlag : Flags)
		{
			Text += bFlag ? TEXT("on ") : TEXT("off ");
		}
		return Text.TrimEnd();
	}

	/** Values that differ from every text, asset and default value in these tests. */
	void SetUnrelatedValues(ACambrianaLiveActor& Actor)
	{
		Actor.Recursion = 2.0;
		Actor.BasicAngle = 91.0;
		Actor.Thickness = 11.0;
		Actor.SizeMode = ECambrianaSizeMode::Fit;
		Actor.SizeValue = 333.0;
		Actor.Sides = 13;
		Actor.bClosedForm = true;
		Actor.MinThickness = 0.5;
	}

	UCambrianaGrammar* MakeGrammar(const FString& Text, ECambrianaSizeMode SizeMode, double SizeValue, int32 Sides, bool bClosedForm, double MinThickness)
	{
		UCambrianaGrammar* Grammar = Cambriana::Tests::MakeGrammar(Text);
		Grammar->SizeMode = SizeMode;
		Grammar->SizeValue = SizeValue;
		Grammar->Sides = Sides;
		Grammar->bClosedForm = bClosedForm;
		Grammar->MinThickness = MinThickness;
		return Grammar;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorPlacementStartsSettingsTest,
	"Cambriana.LiveActor.Placement.SettingsStartFromTheTextThenTheAssetThenTheDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorPlacementStartsSettingsTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaLiveActorPlacementTests;
	struct FCase
	{
		const TCHAR* Label;
		UCambrianaGrammar* Grammar;
		const TCHAR* Expected;
	};
	const FCase Cases[] = {
		{ TEXT(".lsys text stating every setting"),
			MakeGrammar(FString(TEXT("generations 6\nangle 30\nthickness 40%\nsize 50cm\nsides 3\nsurface blocks\nminthickness 5cm\n")) + GrowRule,
				ECambrianaSizeMode::Step, 10.0, 5, true, 20.0),
			TEXT("recursion 6, angle 30, thickness 40, size Fit 50, sides 3, surface blocks, min thickness 5") },
		{ TEXT(".lsys text stating step and surface tubes"),
			MakeGrammar(FString(TEXT("step 2cm\nsurface tubes\n")) + GrowRule, ECambrianaSizeMode::Fit, 80.0, 5, false, 20.0),
			TEXT("recursion 4, angle 25, thickness 50, size Step 2, sides 5, surface tubes, min thickness 20") },
		{ TEXT(".lsys text stating no settings"),
			MakeGrammar(GrowRule, ECambrianaSizeMode::Step, 10.0, 5, false, 20.0),
			TEXT("recursion 4, angle 25, thickness 50, size Step 10, sides 5, surface blocks, min thickness 20") },
		{ TEXT(".ls text"),
			MakeGrammar(TEXT("7\n45\n30\nA\nA=F'A\n@\n"), ECambrianaSizeMode::Step, 10.0, 5, false, 20.0),
			TEXT("recursion 7, angle 45, thickness 30, size Step 10, sides 5, surface blocks, min thickness 20") },
	};
	for (const FCase& Case : Cases)
	{
		FCambrianaTestWorld World;
		ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
		if (!TestNotNull(FString::Printf(TEXT("%s: actor spawned"), Case.Label), Actor))
		{
			continue;
		}
		SetUnrelatedValues(*Actor);
		Actor->PlaceFromGrammar(Case.Grammar);
		TestEqual(FString::Printf(TEXT("%s: the placed actor's settings"), Case.Label), Describe(*Actor), FString(Case.Expected));
		TestEqual(FString::Printf(TEXT("%s: every override stays off"), Case.Label), DescribeOverrides(*Actor), FString(TEXT("off off off off off off off")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorPlacementIndependentTest,
	"Cambriana.LiveActor.Placement.SettingsAreIndependentOfTheAssetAfterwards",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorPlacementIndependentTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaLiveActorPlacementTests;
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(FString(TEXT("generations 6\nsides 3\n")) + GrowRule, ECambrianaSizeMode::Step, 10.0, 5, false, 20.0);
	Actor->PlaceFromGrammar(Grammar);
	const FString Placed = TEXT("recursion 6, angle 25, thickness 50, size Step 10, sides 3, surface blocks, min thickness 20");
	TestEqual(TEXT("The placed actor's settings"), Describe(*Actor), Placed);

	Grammar->GrammarText = FString(TEXT("generations 2\nsides 4\n")) + GrowRule;
	Grammar->SizeMode = ECambrianaSizeMode::Fit;
	Grammar->SizeValue = 90.0;
	Grammar->bClosedForm = true;
	Grammar->MinThickness = 1.0;
	Actor->Rebuild();
	TestEqual(TEXT("Changing the asset's text and settings leaves the actor's settings"), Describe(*Actor), Placed);
	return true;
}

#endif
