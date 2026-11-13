#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestWorld.h"
#include "Algo/Find.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CambrianaLiveActorGapTests
{
	const TCHAR* LegacyText = TEXT("2\n25\n50\nA\nA=F^'A\n@\n");
	const TCHAR* LsysStatingSides = TEXT("sides 3\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	const TCHAR* LsysStatingMinThickness = TEXT("minthickness 2cm\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	const FName SettingRecursion = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Recursion);
	const FName SettingBasicAngle = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle);
	const FName SettingThickness = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Thickness);
	const FName SettingMinThickness = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, MinThickness);
	const FName SettingClosedForm = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm);
	const FName SettingSides = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides);
	const FName SettingSizeMode = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode);
	const FName SettingSizeValue = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue);
	const FName AllSettings[] = { SettingRecursion, SettingBasicAngle, SettingThickness, SettingMinThickness, SettingClosedForm, SettingSides, SettingSizeMode, SettingSizeValue };

	UCambrianaGrammar* NewGrammar(const TCHAR* Text)
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
		Grammar->GrammarText = Text;
		return Grammar;
	}

	/** The settings in a list, in the order AllSettings gives them. */
	FString Joined(const TFunctionRef<bool(FName)>& Includes)
	{
		FString Result;
		for (const FName Name : AllSettings)
		{
			if (Includes(Name))
			{
				Result += Name.ToString() + TEXT(" ");
			}
		}
		return Result.TrimEnd();
	}

	FString UsedNames(const ACambrianaLiveActor& Actor)
	{
		return Joined([&Actor](FName Name) { return Actor.UsesOwnValue(Name); });
	}

	FString Names(std::initializer_list<FName> List)
	{
		return Joined([&List](FName Name) { return Algo::Find(List, Name) != nullptr; });
	}
}

using namespace CambrianaLiveActorGapTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorUsesOwnValueTest,
	"Cambriana.LiveActor.TheBuildUsesTheActorsOwnValueOfASettingExactlyWhenItsOverrideIsOnOrNothingElseGivesIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorUsesOwnValueTest::RunTest(const FString& Parameters)
{
	// SDD-unreal.md, Live actor: an override on uses the actor's value. With it off, recursion, basic angle and thickness come from the
	// text; min thickness, closed form and sides from the text when it states them, else the asset, else the actor without an asset;
	// size from the asset, else, for inline .ls text, the actor.
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("A live actor spawns"), Actor))
	{
		return false;
	}

	Actor->Grammar = nullptr;
	Actor->InlineGrammar = LegacyText;
	TestEqual(TEXT("Inline .ls text"), UsedNames(*Actor), Names({ SettingMinThickness, SettingClosedForm, SettingSides, SettingSizeMode, SettingSizeValue }));
	Actor->InlineGrammar = LsysStatingSides;
	TestEqual(TEXT("Inline .lsys text stating sides"), UsedNames(*Actor), Names({ SettingMinThickness, SettingClosedForm }));
	Actor->InlineGrammar = LsysStatingMinThickness;
	TestEqual(TEXT("Inline .lsys text stating min thickness"), UsedNames(*Actor), Names({ SettingClosedForm, SettingSides }));

	Actor->Grammar = NewGrammar(LsysStatingSides);
	TestEqual(TEXT("An asset holding .lsys text"), UsedNames(*Actor), FString());
	Actor->Grammar = NewGrammar(LegacyText);
	TestEqual(TEXT("An asset holding .ls text"), UsedNames(*Actor), FString());

	TestFalse(TEXT("A name that is not a setting"), Actor->UsesOwnValue(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Seed)));

	struct FOverride
	{
		bool ACambrianaLiveActor::* Flag;
		FName Setting;
	};
	const FOverride Overrides[] = {
		{ &ACambrianaLiveActor::bOverrideRecursion, SettingRecursion }, { &ACambrianaLiveActor::bOverrideBasicAngle, SettingBasicAngle },
		{ &ACambrianaLiveActor::bOverrideThickness, SettingThickness }, { &ACambrianaLiveActor::bOverrideMinThickness, SettingMinThickness },
		{ &ACambrianaLiveActor::bOverrideClosedForm, SettingClosedForm }, { &ACambrianaLiveActor::bOverrideSides, SettingSides },
	};
	for (const FOverride& Override : Overrides)
	{
		Actor->*Override.Flag = true;
		TestEqual(FString::Printf(TEXT("An asset with only the %s override on"), *Override.Setting.ToString()), UsedNames(*Actor), Override.Setting.ToString());
		Actor->*Override.Flag = false;
	}
	Actor->bOverrideSize = true;
	TestEqual(TEXT("An asset with only the size override on"), UsedNames(*Actor), Names({ SettingSizeMode, SettingSizeValue }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorStatedSettingsCaseTest,
	"Cambriana.LiveActor.StartingSettingsFollowATextChangedOnlyInCase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorStatedSettingsCaseTest::RunTest(const FString& Parameters)
{
	// `Generations` is not the setting keyword `generations`, so the second text states no recursion.
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("A live actor spawns"), Actor))
	{
		return false;
	}
	Actor->InlineGrammar = TEXT("generations 3\naxiom F\n");
	TestEqual(TEXT("The text states 3 generations"), Actor->GetStartingSettings().Recursion, 3.0);
	Actor->InlineGrammar = TEXT("Generations 3\naxiom F\n");
	TestEqual(TEXT("The case-changed text states none, so the default 4 applies"), Actor->GetStartingSettings().Recursion, 4.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorFirstBuildTest,
	"Cambriana.LiveActor.AnActorWhoseInputIsTheDefaultStillBuildsOnceWhenItIsConstructed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorFirstBuildTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FTransform Transform;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActorDeferred<ACambrianaLiveActor>(ACambrianaLiveActor::StaticClass(), Transform);
	if (!TestNotNull(TEXT("A live actor spawns"), Actor))
	{
		return false;
	}
	Actor->Seed = 0;
	TestEqual(TEXT("Nothing is built before construction"), Actor->GetBuildCount(), 0);
	TestTrue(TEXT("The input equals a default build input"), Actor->MakeBuildInput() == FCambrianaBuildInput());
	Actor->FinishSpawning(Transform);
	TestEqual(TEXT("Construction builds once"), Actor->GetBuildCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorGrammarChangeTest,
	"Cambriana.LiveActor.AGrammarChangeBuildsOnlyAnActorUsingThatGrammar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorGrammarChangeTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const FTransform Transform;
	UCambrianaGrammar* Used = NewGrammar(LsysStatingSides);
	UCambrianaGrammar* Other = NewGrammar(LsysStatingSides);
	ACambrianaLiveActor* Actor = World.Get()->SpawnActorDeferred<ACambrianaLiveActor>(ACambrianaLiveActor::StaticClass(), Transform);
	if (!TestNotNull(TEXT("A live actor spawns"), Actor))
	{
		return false;
	}
	Actor->Grammar = Used;
	ACambrianaLiveActor::RebuildActorsUsing(Other);
	TestEqual(TEXT("A change to another grammar does not build an actor that was never built"), Actor->GetBuildCount(), 0);
	ACambrianaLiveActor::RebuildActorsUsing(Used);
	TestEqual(TEXT("A change to its grammar builds it"), Actor->GetBuildCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaSettingDefaultsTest,
	"Cambriana.LiveActor.SettingDefaultsAreTheCoresDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaSettingDefaultsTest::RunTest(const FString& Parameters)
{
	// SDD-unreal.md, Live actor: the live actor's class default equals the core's defaults (recursion 4, basic angle 25, thickness 50,
	// Fit 200 cm, sides 8, closed form, min thickness 0); a grammar asset's settings default to the live actor's.
	const ACambrianaLiveActor* Actor = GetDefault<ACambrianaLiveActor>();
	TestEqual(TEXT("Actor recursion"), Actor->Recursion, 4.0);
	TestEqual(TEXT("Actor basic angle"), Actor->BasicAngle, 25.0);
	TestEqual(TEXT("Actor thickness"), Actor->Thickness, 50.0);
	TestTrue(TEXT("Actor size mode is Fit"), Actor->SizeMode == ECambrianaSizeMode::Fit);
	TestEqual(TEXT("Actor size value"), Actor->SizeValue, 200.0);
	TestEqual(TEXT("Actor sides"), Actor->Sides, 8);
	TestTrue(TEXT("Actor closed form"), Actor->bClosedForm);
	TestEqual(TEXT("Actor min thickness"), Actor->MinThickness, 0.0);

	const UCambrianaGrammar* Grammar = GetDefault<UCambrianaGrammar>();
	TestTrue(TEXT("Asset size mode is Fit"), Grammar->SizeMode == ECambrianaSizeMode::Fit);
	TestEqual(TEXT("Asset size value"), Grammar->SizeValue, 200.0);
	TestEqual(TEXT("Asset sides"), Grammar->Sides, 8);
	TestTrue(TEXT("Asset closed form"), Grammar->bClosedForm);
	TestEqual(TEXT("Asset min thickness"), Grammar->MinThickness, 0.0);

	const FCambrianaOverridableSettings Starting = Actor->GetStartingSettings();
	TestEqual(TEXT("Starting recursion"), Starting.Recursion, 4.0);
	TestEqual(TEXT("Starting basic angle"), Starting.BasicAngle, 25.0);
	TestEqual(TEXT("Starting thickness"), Starting.Thickness, 50.0);
	TestEqual(TEXT("Starting size value"), Starting.SizeValue, 200.0);
	TestEqual(TEXT("Starting sides"), Starting.Sides, 8);
	TestEqual(TEXT("Starting min thickness"), Starting.MinThickness, 0.0);
	return true;
}

#endif
