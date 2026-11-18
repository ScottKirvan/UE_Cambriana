#include "CambrianaBuild.h"
#include "CambrianaGrammarConversion.h"
#include "CambrianaTestUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CambrianaStatedSettingsTests
{
	FString Describe(const FCambrianaStatedSettings& Stated)
	{
		const auto Number = [](const auto& Value) { return Value ? FString::Printf(TEXT("%g"), double(*Value)) : FString(TEXT("unset")); };
		const FString Mode = Stated.SizeMode ? (*Stated.SizeMode == ECambrianaSizeMode::Fit ? TEXT("Fit") : TEXT("Step")) : TEXT("unset");
		const FString ClosedForm = Stated.bClosedForm ? (*Stated.bClosedForm ? TEXT("tubes") : TEXT("blocks")) : TEXT("unset");
		return FString::Printf(TEXT("recursion %s, angle %s, thickness %s, size %s %s, sides %s, surface %s, min thickness %s"),
			*Number(Stated.Recursion), *Number(Stated.BasicAngle), *Number(Stated.Thickness), *Mode, *Number(Stated.SizeValue),
			*Number(Stated.Sides), *ClosedForm, *Number(Stated.MinThickness));
	}

	const TCHAR* GrowRule = TEXT("axiom A\n\ngrow {\n    A -> F A\n}\n");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaStatedSettingsLsysTest,
	"Cambriana.Build.StatedSettings.LsysTextStatesItsSettingsStatements",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaStatedSettingsLsysTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaStatedSettingsTests;
	struct FCase
	{
		FString Text;
		const TCHAR* Expected;
	};
	const FCase Cases[] = {
		{ FString(TEXT("generations 6\nangle 30\nthickness 40%\nsize 50cm\nsides 3\nsurface blocks\nminthickness 5cm\n")) + GrowRule,
			TEXT("recursion 6, angle 30, thickness 40, size Fit 50, sides 3, surface blocks, min thickness 5") },
		{ FString(TEXT("step 2cm\nsurface tubes\n")) + GrowRule,
			TEXT("recursion unset, angle unset, thickness unset, size Step 2, sides unset, surface tubes, min thickness unset") },
		{ FString(GrowRule),
			TEXT("recursion unset, angle unset, thickness unset, size unset unset, sides unset, surface unset, min thickness unset") },
		{ FString(),
			TEXT("recursion unset, angle unset, thickness unset, size unset unset, sides unset, surface unset, min thickness unset") },
	};
	for (const FCase& Case : Cases)
	{
		TestEqual(FString::Printf(TEXT("Settings stated by \"%s\""), *Case.Text.Left(40).ReplaceCharWithEscapedChar()),
			Describe(Cambriana::ReadStatedSettings(Case.Text)), FString(Case.Expected));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaStatedSettingsLsTest,
	"Cambriana.Build.StatedSettings.LsTextStatesItsHeader",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaStatedSettingsLsTest::RunTest(const FString& Parameters)
{
	using namespace CambrianaStatedSettingsTests;
	const FString Sk = Cambriana::Tests::LoadFixture(TEXT("sk.ls"));
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Sk.IsEmpty()))
	{
		return false;
	}
	TestEqual(TEXT("sk.ls states its header's recursion, angle and thickness"), Describe(Cambriana::ReadStatedSettings(Sk)),
		FString(TEXT("recursion 20, angle 25, thickness 50, size unset unset, sides unset, surface unset, min thickness unset")));
	TestEqual(TEXT("A .ls header of 7, 45 and 30 is stated"), Describe(Cambriana::ReadStatedSettings(TEXT("7\n45\n30\nA\nA=F'A\n@\n"))),
		FString(TEXT("recursion 7, angle 45, thickness 30, size unset unset, sides unset, surface unset, min thickness unset")));
	TestEqual(TEXT(".ls text with errors states nothing"), Describe(Cambriana::ReadStatedSettings(TEXT("7\n45\n"))),
		FString(TEXT("recursion unset, angle unset, thickness unset, size unset unset, sides unset, surface unset, min thickness unset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarSeedTest,
	"Cambriana.Build.GrammarSeedIsTheTextsWholeNumberFromZeroToFourBillion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarSeedTest::RunTest(const FString& Parameters)
{
	// FMT-grammar.md: `seed` is a whole number from 0 to 4294967295.
	struct FCase
	{
		const TCHAR* Text;
		TOptional<uint32> Expected;
	};
	const FCase Cases[] = {
		{ TEXT("seed 0\naxiom F\n"), 0u },
		{ TEXT("seed 7\naxiom F\n"), 7u },
		{ TEXT("seed 4294967295\naxiom F\n"), 4294967295u },
		{ TEXT("seed 1.5\naxiom F\n"), {} },
		{ TEXT("seed -1\naxiom F\n"), {} },
		{ TEXT("seed 4294967296\naxiom F\n"), {} },
		{ TEXT("axiom F\n"), {} },
	};
	for (const FCase& Case : Cases)
	{
		const TOptional<uint32> Seed = Cambriana::GrammarSeed(Case.Text);
		const FString Label = FString(Case.Text).Left(18).ReplaceCharWithEscapedChar();
		TestEqual(FString::Printf(TEXT("\"%s\" gives a seed"), *Label), Seed.IsSet(), Case.Expected.IsSet());
		if (Seed && Case.Expected)
		{
			TestEqual(FString::Printf(TEXT("\"%s\" gives its number"), *Label), *Seed, *Case.Expected);
		}
	}
	return true;
}

#endif
