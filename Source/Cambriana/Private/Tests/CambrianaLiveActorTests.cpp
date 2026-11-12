#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UCambrianaGrammar* MakeGrammar(const FString& Text)
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
		Grammar->GrammarText = Text;
		return Grammar;
	}
}

using Cambriana::Tests::SimpleGrammar;
#if WITH_EDITOR
using Cambriana::Tests::EditProperty;
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorNewSeedsTest,
	"Cambriana.LiveActor.NewActorsGetDistinctGeneratedSeeds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorNewSeedsTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const ACambrianaLiveActor* First = World.Get()->SpawnActor<ACambrianaLiveActor>();
	const ACambrianaLiveActor* Second = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("First actor spawned"), First) || !TestNotNull(TEXT("Second actor spawned"), Second))
	{
		return false;
	}
	TestNull(TEXT("First actor has no grammar asset"), First->Grammar.Get());
	TestNull(TEXT("Second actor has no grammar asset"), Second->Grammar.Get());
	TestNotEqual(TEXT("The seeds differ"), First->Seed, Second->Seed);
	TestEqual(TEXT("First actor was built with its seed"), First->GetLastBuildInput().Seed, First->Seed);
	TestEqual(TEXT("Second actor was built with its seed"), Second->GetLastBuildInput().Seed, Second->Seed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorPlaceFromGrammarTest,
	"Cambriana.LiveActor.PlacingFromGrammarCopiesItsSeed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorPlaceFromGrammarTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* FromAsset = World.Get()->SpawnActor<ACambrianaLiveActor>();
	ACambrianaLiveActor* WithoutAsset = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), FromAsset) || !TestNotNull(TEXT("Second actor spawned"), WithoutAsset))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(SimpleGrammar(2));
	Grammar->Seed = FromAsset->Seed + 1;

	FromAsset->PlaceFromGrammar(Grammar);
	TestTrue(TEXT("The actor references the asset"), FromAsset->Grammar == Grammar);
	TestEqual(TEXT("The actor has the asset's seed"), FromAsset->Seed, Grammar->Seed);
	TestEqual(TEXT("The actor was built with that seed"), FromAsset->GetLastBuildInput().Seed, Grammar->Seed);

	const uint32 GeneratedSeed = WithoutAsset->Seed;
	WithoutAsset->PlaceFromGrammar(nullptr);
	TestEqual(TEXT("Without an asset the generated seed is kept"), WithoutAsset->Seed, GeneratedSeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBuildsOnSpawnTest,
	"Cambriana.LiveActor.BuildsWhenSpawned",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBuildsOnSpawnTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	const ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (TestNotNull(TEXT("Actor spawned"), Actor))
	{
		TestTrue(TEXT("Actor built at least once"), Actor->GetBuildCount() > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorGrammarTextSourceTest,
	"Cambriana.LiveActor.GrammarAssetTextTakesPrecedenceOverInlineText",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorGrammarTextSourceTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	Actor->InlineGrammar = SimpleGrammar(1);
	TestEqualSensitive(TEXT("Inline text is used without a grammar asset"), Actor->MakeBuildInput().GrammarText, SimpleGrammar(1));

	Actor->SetGrammar(MakeGrammar(SimpleGrammar(2)));
	TestEqualSensitive(TEXT("Asset text is used with a grammar asset"), Actor->GetLastBuildInput().GrammarText, SimpleGrammar(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSettingsTest,
	"Cambriana.LiveActor.BuildInputCarriesSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSettingsTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}

	const FCambrianaBuildSettings Defaults = Actor->MakeBuildInput().Settings;
	TestFalse(TEXT("Recursion comes from the grammar by default"), Defaults.Recursion.IsSet());
	TestFalse(TEXT("Basic angle comes from the grammar by default"), Defaults.BasicAngle.IsSet());
	TestFalse(TEXT("Thickness comes from the grammar by default"), Defaults.Thickness.IsSet());
	TestEqual(TEXT("Default min thickness"), Defaults.MinThickness, 0.0);
	TestTrue(TEXT("Closed form by default"), Defaults.bClosedForm);
	TestEqual(TEXT("Default sides"), Defaults.Sides, 8);
	TestTrue(TEXT("Fit by default"), Defaults.SizeMode == ECambrianaSizeMode::Fit);
	TestEqual(TEXT("Default target size"), Defaults.SizeValue, 200.0);
	TestFalse(TEXT("A grammar's size applies by default"), Defaults.bOverrideGrammarSize);

	Actor->bOverrideRecursion = true;
	Actor->Recursion = 2.5;
	Actor->bOverrideBasicAngle = true;
	Actor->BasicAngle = 30.0;
	Actor->bOverrideThickness = true;
	Actor->Thickness = 40.0;
	Actor->MinThickness = 0.5;
	Actor->bClosedForm = false;
	Actor->Sides = 5;
	Actor->SizeMode = ECambrianaSizeMode::Step;
	Actor->SizeValue = 12.0;
	Actor->bOverrideSize = true;
	Actor->Seed = 77;

	const FCambrianaBuildInput Input = Actor->MakeBuildInput();
	TestEqual(TEXT("Recursion override"), Input.Settings.Recursion.Get(0.0), 2.5);
	TestEqual(TEXT("Basic angle override"), Input.Settings.BasicAngle.Get(0.0), 30.0);
	TestEqual(TEXT("Thickness override"), Input.Settings.Thickness.Get(0.0), 40.0);
	TestEqual(TEXT("Min thickness"), Input.Settings.MinThickness, 0.5);
	TestFalse(TEXT("Closed form"), Input.Settings.bClosedForm);
	TestEqual(TEXT("Sides"), Input.Settings.Sides, 5);
	TestTrue(TEXT("Size mode"), Input.Settings.SizeMode == ECambrianaSizeMode::Step);
	TestEqual(TEXT("Size value"), Input.Settings.SizeValue, 12.0);
	TestTrue(TEXT("Size override"), Input.Settings.bOverrideGrammarSize);
	TestEqual(TEXT("Seed"), Input.Seed, 77u);
	return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorRebuildsOnGrammarChangeTest,
	"Cambriana.LiveActor.GrammarAssetChangeRebuildsReferencingActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorRebuildsOnGrammarChangeTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	UCambrianaGrammar* Grammar = MakeGrammar(SimpleGrammar(3));
	UCambrianaGrammar* OtherGrammar = MakeGrammar(SimpleGrammar(4));
	ACambrianaLiveActor* First = World.Get()->SpawnActor<ACambrianaLiveActor>();
	ACambrianaLiveActor* Second = World.Get()->SpawnActor<ACambrianaLiveActor>();
	ACambrianaLiveActor* Unrelated = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("First actor spawned"), First) || !TestNotNull(TEXT("Second actor spawned"), Second)
		|| !TestNotNull(TEXT("Unrelated actor spawned"), Unrelated))
	{
		return false;
	}
	First->SetGrammar(Grammar);
	Second->SetGrammar(Grammar);
	Unrelated->SetGrammar(OtherGrammar);
	const int32 FirstCount = First->GetBuildCount();
	const int32 SecondCount = Second->GetBuildCount();
	const int32 UnrelatedCount = Unrelated->GetBuildCount();

	EditProperty(Grammar, GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText), [Grammar] { Grammar->GrammarText = SimpleGrammar(5); });

	TestEqual(TEXT("First referencing actor rebuilt once"), First->GetBuildCount(), FirstCount + 1);
	TestEqual(TEXT("Second referencing actor rebuilt once"), Second->GetBuildCount(), SecondCount + 1);
	TestEqualSensitive(TEXT("First actor built from the changed text"), First->GetLastBuildInput().GrammarText, SimpleGrammar(5));
	TestEqualSensitive(TEXT("Second actor built from the changed text"), Second->GetLastBuildInput().GrammarText, SimpleGrammar(5));
	TestEqual(TEXT("Actor referencing another asset did not rebuild"), Unrelated->GetBuildCount(), UnrelatedCount);

	const int32 FirstCountBeforeSeed = First->GetBuildCount();
	EditProperty(Grammar, GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed), [Grammar] { Grammar->Seed += 1; });
	TestEqual(TEXT("An asset seed change does not rebuild actors, which build with their own seed"), First->GetBuildCount(), FirstCountBeforeSeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorRebuildsOnPropertyChangeTest,
	"Cambriana.LiveActor.PropertyChangeRebuildsActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorRebuildsOnPropertyChangeTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}

	const auto ExpectRebuild = [this, Actor](FName PropertyName, TFunctionRef<void()> Change)
	{
		const int32 Before = Actor->GetBuildCount();
		EditProperty(Actor, PropertyName, Change);
		TestTrue(FString::Printf(TEXT("Changing %s rebuilds the actor"), *PropertyName.ToString()), Actor->GetBuildCount() > Before);
		TestTrue(FString::Printf(TEXT("Build after %s used the current properties"), *PropertyName.ToString()),
			Actor->GetLastBuildInput() == Actor->MakeBuildInput());
	};

	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, InlineGrammar), [Actor] { Actor->InlineGrammar = SimpleGrammar(6); });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Seed), [Actor] { Actor->Seed += 1; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle), [Actor] { Actor->bOverrideBasicAngle = true; Actor->BasicAngle = 33.0; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides), [Actor] { Actor->Sides = 6; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode), [Actor] { Actor->SizeMode = ECambrianaSizeMode::Step; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideSize), [Actor] { Actor->bOverrideSize = true; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Grammar), [Actor] { Actor->Grammar = MakeGrammar(SimpleGrammar(2)); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorMoveDoesNotRebuildTest,
	"Cambriana.LiveActor.MovingActorDoesNotRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorMoveDoesNotRebuildTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	Actor->SetGrammar(MakeGrammar(SimpleGrammar(2)));
	const int32 Before = Actor->GetBuildCount();

	Actor->SetActorLocation(FVector(100.0, 200.0, 300.0));
	Actor->PostEditMove(false);
	Actor->PostEditMove(true);

	FProperty* LocationProperty = USceneComponent::StaticClass()->FindPropertyByName(USceneComponent::GetRelativeLocationPropertyName());
	FPropertyChangedEvent LocationEvent(LocationProperty, EPropertyChangeType::ValueSet);
	Actor->PostEditChangeProperty(LocationEvent);

	TestEqual(TEXT("A transform-only change does not rebuild"), Actor->GetBuildCount(), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorUndoRebuildsTest,
	"Cambriana.LiveActor.UndoRebuildsActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorUndoRebuildsTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	const int32 OriginalSides = Actor->Sides;
	EditProperty(Actor, GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides), [Actor] { Actor->Sides = 5; });
	const int32 Before = Actor->GetBuildCount();

	Actor->Sides = OriginalSides;
	Actor->PostEditUndo();

	TestTrue(TEXT("Undo rebuilds the actor"), Actor->GetBuildCount() > Before);
	TestEqual(TEXT("Build after undo used the restored properties"), Actor->GetLastBuildInput().Settings.Sides, OriginalSides);
	return true;
}

#endif

#endif
