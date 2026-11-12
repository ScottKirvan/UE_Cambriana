#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::LoadFixture;

namespace
{
	enum class ETextSource
	{
		Inline,
		Asset,
	};

	struct FOverride
	{
		const TCHAR* Name;
		bool ACambrianaLiveActor::*Flag;
	};

	const FOverride Overrides[] = {
		{ TEXT("recursion"), &ACambrianaLiveActor::bOverrideRecursion },
		{ TEXT("basic angle"), &ACambrianaLiveActor::bOverrideBasicAngle },
		{ TEXT("thickness"), &ACambrianaLiveActor::bOverrideThickness },
		{ TEXT("size"), &ACambrianaLiveActor::bOverrideSize },
	};

	TArray<FVector3d> PositionsOf(const ACambrianaLiveActor* Actor)
	{
		TArray<FVector3d> Positions;
		const UE::Geometry::FDynamicMesh3& Mesh = *Actor->GetDynamicMeshComponent()->GetMesh();
		for (const int32 Vid : Mesh.VertexIndicesItr())
		{
			Positions.Add(Mesh.GetVertex(Vid));
		}
		return Positions;
	}

	ACambrianaLiveActor* SpawnWithStaleSettings(UWorld* World)
	{
		ACambrianaLiveActor* Actor = World->SpawnActor<ACambrianaLiveActor>();
		if (Actor)
		{
			Actor->Seed = 2468;
			Actor->Recursion = 3.0;
			Actor->BasicAngle = 90.0;
			Actor->Thickness = 10.0;
			Actor->SizeMode = ECambrianaSizeMode::Step;
			Actor->SizeValue = 7.0;
		}
		return Actor;
	}

	void BuildFrom(ACambrianaLiveActor* Actor, ETextSource Source, const FString& Text)
	{
		if (Source == ETextSource::Asset)
		{
			UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
			Grammar->GrammarText = Text;
			Actor->SetGrammar(Grammar);
		}
		else
		{
			Actor->InlineGrammar = Text;
			Actor->Rebuild();
		}
	}

	void ExpectOverridesChangeNothing(FAutomationTestBase& Test, const FString& Label, const FString& Text)
	{
		for (const ETextSource Source : { ETextSource::Inline, ETextSource::Asset })
		{
			const FString Case = FString::Printf(TEXT("%s from %s"), *Label, Source == ETextSource::Asset ? TEXT("an asset") : TEXT("inline text"));
			FCambrianaTestWorld World;
			ACambrianaLiveActor* Actor = SpawnWithStaleSettings(World.Get());
			if (!Test.TestNotNull(Case + TEXT(": actor spawned"), Actor))
			{
				continue;
			}
			BuildFrom(Actor, Source, Text);
			const FCambrianaBuildReport& Report = Actor->GetLastBuildReport();
			Test.TestFalse(Case + TEXT(": no errors"), Report.bHasErrors);
			Test.TestEqual(Case + TEXT(": shown recursion is the one used"), Actor->Recursion, Report.Recursion);
			Test.TestEqual(Case + TEXT(": shown basic angle is the one used"), Actor->BasicAngle, Report.BasicAngle);
			Test.TestEqual(Case + TEXT(": shown thickness is the one used"), Actor->Thickness, Report.Thickness);
			if (Actor->SizeMode == ECambrianaSizeMode::Step)
			{
				Test.TestEqual(Case + TEXT(": shown step length is the one used"), Actor->SizeValue, Report.StepLength);
			}

			const TArray<FVector3d> Baseline = PositionsOf(Actor);
			Test.TestTrue(Case + TEXT(": built geometry"), Baseline.Num() > 0);
			for (const FOverride& Override : Overrides)
			{
				Actor->*Override.Flag = true;
				Actor->Rebuild();
				Test.TestTrue(FString::Printf(TEXT("%s: turning the %s override on leaves the vertex positions unchanged"), *Case, Override.Name),
					PositionsOf(Actor) == Baseline);
				Actor->*Override.Flag = false;
				Actor->Rebuild();
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorShownSettingsSkLsTest,
	"Cambriana.LiveActor.ShownSettings.SkLsOverridesTurnedOnChangeNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorShownSettingsSkLsTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.ls"));
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}
	ExpectOverridesChangeNothing(*this, TEXT("sk.ls"), Text);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorShownSettingsSkLsysTest,
	"Cambriana.LiveActor.ShownSettings.SkLsysOverridesTurnedOnChangeNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorShownSettingsSkLsysTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}
	ExpectOverridesChangeNothing(*this, TEXT("sk.lsys"), Text);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorShownSettingsGrammarSizeTest,
	"Cambriana.LiveActor.ShownSettings.LsysSizeOrStepIsShownWithItsMode",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorShownSettingsGrammarSizeTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}

	struct FCase
	{
		const TCHAR* Setting;
		ECambrianaSizeMode Mode;
		double Value;
	};
	for (const FCase& Case : { FCase{ TEXT("size 50cm"), ECambrianaSizeMode::Fit, 50.0 }, FCase{ TEXT("step 2cm"), ECambrianaSizeMode::Step, 2.0 },
			 FCase{ TEXT(""), ECambrianaSizeMode::Fit, 200.0 } })
	{
		const FString Label = FString::Printf(TEXT("sk.lsys with \"%s\""), Case.Setting);
		const FString CaseText = FString(Case.Setting) + TEXT("\n") + Text;
		FCambrianaTestWorld World;
		ACambrianaLiveActor* Actor = SpawnWithStaleSettings(World.Get());
		if (!TestNotNull(Label + TEXT(": actor spawned"), Actor))
		{
			continue;
		}
		BuildFrom(Actor, ETextSource::Inline, CaseText);
		TestTrue(Label + TEXT(": the shown size mode is the grammar's"), Actor->SizeMode == Case.Mode);
		TestNearlyEqual(Label + TEXT(": the shown size value is the grammar's"), Actor->SizeValue, Case.Value, 1e-9);
		ExpectOverridesChangeNothing(*this, Label, CaseText);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorShownSettingsGrammarSurfacerTest,
	"Cambriana.LiveActor.ShownSettings.LsysSurfacerSettingsAreShownAndOverridesTurnedOnChangeNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorShownSettingsGrammarSurfacerTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}

	const FOverride SurfacerOverrides[] = {
		{ TEXT("sides"), &ACambrianaLiveActor::bOverrideSides },
		{ TEXT("closed form"), &ACambrianaLiveActor::bOverrideClosedForm },
		{ TEXT("min thickness"), &ACambrianaLiveActor::bOverrideMinThickness },
	};
	struct FCase
	{
		const TCHAR* Setting;
		int32 Sides;
		bool bClosedForm;
		double MinThickness;
	};
	for (const FCase& Case : { FCase{ TEXT("sides 3"), 3, true, 0.0 }, FCase{ TEXT("surface blocks"), 8, false, 0.0 },
			 FCase{ TEXT("minthickness 5cm"), 8, true, 5.0 } })
	{
		for (const ETextSource Source : { ETextSource::Inline, ETextSource::Asset })
		{
			const FString Label = FString::Printf(TEXT("sk.lsys with \"%s\" from %s"), Case.Setting, Source == ETextSource::Asset ? TEXT("an asset") : TEXT("inline text"));
			FCambrianaTestWorld World;
			ACambrianaLiveActor* Actor = SpawnWithStaleSettings(World.Get());
			if (!TestNotNull(Label + TEXT(": actor spawned"), Actor))
			{
				continue;
			}
			Actor->Sides = 8;
			Actor->bClosedForm = true;
			Actor->MinThickness = 0.0;
			BuildFrom(Actor, Source, FString(Case.Setting) + TEXT("\nstep 2cm\n") + Text);
			TestFalse(Label + TEXT(": no errors"), Actor->GetLastBuildReport().bHasErrors);
			TestEqual(Label + TEXT(": shown sides"), Actor->Sides, Case.Sides);
			TestEqual(Label + TEXT(": shown closed form"), Actor->bClosedForm, Case.bClosedForm);
			TestEqual(Label + TEXT(": shown min thickness"), Actor->MinThickness, Case.MinThickness);

			const TArray<FVector3d> Baseline = PositionsOf(Actor);
			TestTrue(Label + TEXT(": built geometry"), Baseline.Num() > 0);
			for (const FOverride& Override : SurfacerOverrides)
			{
				Actor->*Override.Flag = true;
				Actor->Rebuild();
				TestTrue(FString::Printf(TEXT("%s: turning the %s override on leaves the vertex positions unchanged"), *Label, Override.Name),
					PositionsOf(Actor) == Baseline);
				Actor->*Override.Flag = false;
				Actor->Rebuild();
			}
		}
	}
	return true;
}

#endif
