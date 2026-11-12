#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::LoadFixture;

namespace CambrianaLiveActorOverrideTests
{
	enum class ETextSource
	{
		Inline,
		Asset,
	};

	FString Number(double Value)
	{
		return FString::Printf(TEXT("%g"), Value);
	}

	FString Bool(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString Size(ECambrianaSizeMode Mode, double Value)
	{
		return FString::Printf(TEXT("%s %g"), Mode == ECambrianaSizeMode::Fit ? TEXT("Fit") : TEXT("Step"), Value);
	}

	/** One overridable setting: the actor's value and the value a build used, both as text. */
	struct FSetting
	{
		const TCHAR* Name;
		bool ACambrianaLiveActor::*Flag;
		FString (*ActorValue)(const ACambrianaLiveActor&);
		FString (*UsedValue)(const FCambrianaBuildReport&);
	};

	const FSetting Recursion{ TEXT("recursion"), &ACambrianaLiveActor::bOverrideRecursion,
		[](const ACambrianaLiveActor& Actor) { return Number(Actor.Recursion); },
		[](const FCambrianaBuildReport& Report) { return Number(Report.Recursion); } };
	const FSetting BasicAngle{ TEXT("basic angle"), &ACambrianaLiveActor::bOverrideBasicAngle,
		[](const ACambrianaLiveActor& Actor) { return Number(Actor.BasicAngle); },
		[](const FCambrianaBuildReport& Report) { return Number(Report.BasicAngle); } };
	const FSetting Thickness{ TEXT("thickness"), &ACambrianaLiveActor::bOverrideThickness,
		[](const ACambrianaLiveActor& Actor) { return Number(Actor.Thickness); },
		[](const FCambrianaBuildReport& Report) { return Number(Report.Thickness); } };
	const FSetting SizeSetting{ TEXT("size"), &ACambrianaLiveActor::bOverrideSize,
		[](const ACambrianaLiveActor& Actor) { return Size(Actor.SizeMode, Actor.SizeValue); },
		[](const FCambrianaBuildReport& Report) { return Size(Report.SizeMode, Report.SizeValue); } };
	const FSetting Sides{ TEXT("sides"), &ACambrianaLiveActor::bOverrideSides,
		[](const ACambrianaLiveActor& Actor) { return Number(Actor.Sides); },
		[](const FCambrianaBuildReport& Report) { return Number(Report.Sides); } };
	const FSetting ClosedForm{ TEXT("closed form"), &ACambrianaLiveActor::bOverrideClosedForm,
		[](const ACambrianaLiveActor& Actor) { return Bool(Actor.bClosedForm); },
		[](const FCambrianaBuildReport& Report) { return Bool(Report.bClosedForm); } };
	const FSetting MinThickness{ TEXT("min thickness"), &ACambrianaLiveActor::bOverrideMinThickness,
		[](const ACambrianaLiveActor& Actor) { return Number(Actor.MinThickness); },
		[](const FCambrianaBuildReport& Report) { return Number(Report.MinThickness); } };

	/** A setting, the value the actor keeps, and the value the build takes from the text, asset or default with the override off. */
	struct FCase
	{
		const FSetting& Setting;
		const TCHAR* Kept;
		const TCHAR* Source;
	};

	/** Each differs from the text, asset or default value its case's build takes. */
	void SetKeptValues(ACambrianaLiveActor& Actor)
	{
		Actor.Recursion = 3.0;
		Actor.BasicAngle = 90.0;
		Actor.Thickness = 10.0;
		Actor.SizeMode = ECambrianaSizeMode::Step;
		Actor.SizeValue = 7.0;
		Actor.Sides = 5;
		Actor.bClosedForm = true;
		Actor.MinThickness = 2.0;
	}

	const TCHAR* Describe(ETextSource Source)
	{
		return Source == ETextSource::Asset ? TEXT("from an asset") : TEXT("inline");
	}

	void BuildFrom(ACambrianaLiveActor& Actor, ETextSource Source, const FString& Text)
	{
		if (Source == ETextSource::Asset)
		{
			UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
			Grammar->GrammarText = Text;
			Actor.SetGrammar(Grammar);
		}
		else
		{
			Actor.InlineGrammar = Text;
			Actor.Rebuild();
		}
	}

	void ExpectOverridesKeepActorValues(FAutomationTestBase& Test, const FString& Label, ETextSource Source, const FString& Text,
		TConstArrayView<FCase> Cases)
	{
		const FString Prefix = FString::Printf(TEXT("%s %s"), *Label, Describe(Source));
		FCambrianaTestWorld World;
		ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
		if (!Test.TestNotNull(Prefix + TEXT(": actor spawned"), Actor))
		{
			return;
		}
		SetKeptValues(*Actor);
		BuildFrom(*Actor, Source, Text);
		Test.TestFalse(Prefix + TEXT(": no errors"), Actor->GetLastBuildReport().bHasErrors);

		for (const FCase& Case : Cases)
		{
			const FString Name = FString::Printf(TEXT("%s, %s"), *Prefix, Case.Setting.Name);
			Test.TestEqual(Name + TEXT(": building with the override off keeps the actor's value"), Case.Setting.ActorValue(*Actor), FString(Case.Kept));
			Test.TestEqual(Name + TEXT(": the build with the override off uses the text, asset or default"),
				Case.Setting.UsedValue(Actor->GetLastBuildReport()), FString(Case.Source));

			Actor->*Case.Setting.Flag = true;
			Actor->Rebuild();
			Test.TestEqual(Name + TEXT(": turning the override on builds with the actor's value"),
				Case.Setting.UsedValue(Actor->GetLastBuildReport()), FString(Case.Kept));

			Actor->*Case.Setting.Flag = false;
			Actor->Rebuild();
			Test.TestEqual(Name + TEXT(": turning the override off keeps the actor's value"), Case.Setting.ActorValue(*Actor), FString(Case.Kept));
			Test.TestEqual(Name + TEXT(": turning the override off builds with the text, asset or default"),
				Case.Setting.UsedValue(Actor->GetLastBuildReport()), FString(Case.Source));

			Actor->*Case.Setting.Flag = true;
			Actor->Rebuild();
			Test.TestEqual(Name + TEXT(": turning the override back on builds with the actor's kept value"),
				Case.Setting.UsedValue(Actor->GetLastBuildReport()), FString(Case.Kept));
			Actor->*Case.Setting.Flag = false;
			Actor->Rebuild();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorOverridesKeepValuesLsysTest,
	"Cambriana.LiveActor.Overrides.LsysTurningAnOverrideOffKeepsTheActorsValue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorOverridesKeepValuesLsysTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaLiveActorOverrideTests;
	const FString Fixture = LoadFixture(TEXT("sk.lsys"));
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Fixture.IsEmpty()))
	{
		return false;
	}
	const FString Text = TEXT("size 50cm\nsides 3\nsurface blocks\nminthickness 5cm\n") + Fixture;
	const FCase Cases[] = {
		{ Recursion, TEXT("3"), TEXT("20") },
		{ BasicAngle, TEXT("90"), TEXT("25") },
		{ Thickness, TEXT("10"), TEXT("50") },
		{ SizeSetting, TEXT("Step 7"), TEXT("Fit 50") },
		{ Sides, TEXT("5"), TEXT("3") },
		{ ClosedForm, TEXT("true"), TEXT("false") },
		{ MinThickness, TEXT("2"), TEXT("5") },
	};
	for (const ETextSource Source : { ETextSource::Inline, ETextSource::Asset })
	{
		ExpectOverridesKeepActorValues(*this, TEXT("sk.lsys with size and surfacer settings"), Source, Text, Cases);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorOverridesKeepValuesLsTest,
	"Cambriana.LiveActor.Overrides.LsTurningAnOverrideOffKeepsTheActorsValue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorOverridesKeepValuesLsTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaLiveActorOverrideTests;
	const FString Text = LoadFixture(TEXT("sk.ls"));
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}
	const FCase Cases[] = {
		{ Recursion, TEXT("3"), TEXT("20") },
		{ BasicAngle, TEXT("90"), TEXT("25") },
		{ Thickness, TEXT("10"), TEXT("50") },
	};
	for (const ETextSource Source : { ETextSource::Inline, ETextSource::Asset })
	{
		ExpectOverridesKeepActorValues(*this, TEXT("sk.ls"), Source, Text, Cases);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorOverridesKeepValuesDefaultTest,
	"Cambriana.LiveActor.Overrides.DefaultsTurningAnOverrideOffKeepsTheActorsValue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorOverridesKeepValuesDefaultTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaLiveActorOverrideTests;
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}
	const FCase Cases[] = {
		{ SizeSetting, TEXT("Step 7"), TEXT("Fit 200") },
		{ Sides, TEXT("5"), TEXT("8") },
		{ MinThickness, TEXT("2"), TEXT("0") },
	};
	ExpectOverridesKeepActorValues(*this, TEXT("sk.lsys without size or surfacer settings"), ETextSource::Asset, Text, Cases);
	return true;
}

#endif
