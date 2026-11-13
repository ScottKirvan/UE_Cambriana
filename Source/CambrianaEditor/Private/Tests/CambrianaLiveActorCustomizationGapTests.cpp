#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaLiveActorCustomization.h"
#include "Editor.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::DestroyActor;
using Cambriana::Editor::Tests::FindNode;
using Cambriana::Editor::Tests::FindWidgetOfType;
using Cambriana::Editor::Tests::SpawnInlineActor;
using Cambriana::Editor::Tests::PlaceInCurrentLevel;
using Cambriana::Editor::Tests::MakeRows;

namespace
{
	const TCHAR* SidesStatingText = TEXT("sides 3\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	const TCHAR* SidesLeavingText = TEXT("axiom A\n\ngrow {\n    A -> F A\n}\n");

	UCambrianaGrammar* GrammarWithText(const TCHAR* Text)
	{
		UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
		Grammar->GrammarText = Text;
		return Grammar;
	}

	TSharedPtr<IPropertyHandle> HandleOf(const IPropertyRowGenerator& Rows, FName PropertyName)
	{
		const TSharedPtr<IDetailTreeNode> Node = FindNode(Rows.GetRootTreeNodes(), PropertyName);
		return Node ? Node->CreatePropertyHandle() : nullptr;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSelectionTest,
	"Cambriana.Editor.LiveActorDetails.WithSeveralActorsSelectedASettingIsUsedAndDiffersWhenAnyOfThemUsesOrDiffers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSelectionTest::RunTest(const FString& Parameters)
{
	const FName Sides = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides);
	ACambrianaLiveActor* Inline = SpawnInlineActor(SidesLeavingText);
	ACambrianaLiveActor* AtStart = PlaceInCurrentLevel(GrammarWithText(SidesStatingText));
	ACambrianaLiveActor* Moved = PlaceInCurrentLevel(GrammarWithText(SidesStatingText));
	if (!TestNotNull(TEXT("An inline actor spawns"), Inline) || !TestNotNull(TEXT("An actor is placed from a grammar"), AtStart)
		|| !TestNotNull(TEXT("A second actor is placed from a grammar"), Moved))
	{
		DestroyActor(Inline);
		DestroyActor(AtStart);
		DestroyActor(Moved);
		return false;
	}
	Moved->Sides = 9;

	TestFalse(TEXT("A text stating sides gives the actor the text's sides"), AtStart->UsesOwnValue(Sides));
	TestTrue(TEXT("Inline text that states no sides uses the actor's"), Inline->UsesOwnValue(Sides));
	for (const bool bInlineFirst : { true, false })
	{
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows(bInlineFirst ? TArray<UObject*>{ Inline, AtStart } : TArray<UObject*>{ AtStart, Inline });
		const TSharedPtr<IPropertyHandle> Handle = HandleOf(*Rows, Sides);
		if (TestTrue(TEXT("Sides has a Details row"), Handle.IsValid()))
		{
			TestTrue(FString::Printf(TEXT("The build uses the actors' own value when any selected actor does (inline %s)"), bInlineFirst ? TEXT("first") : TEXT("last")),
				Cambriana::Editor::UsesOwnValue(*Handle));
		}
	}
	for (const bool bMovedFirst : { true, false })
	{
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows(bMovedFirst ? TArray<UObject*>{ Moved, AtStart } : TArray<UObject*>{ AtStart, Moved });
		const TSharedPtr<IPropertyHandle> Handle = HandleOf(*Rows, Sides);
		if (TestTrue(TEXT("Sides has a Details row"), Handle.IsValid()))
		{
			TestTrue(FString::Printf(TEXT("The value differs when any selected actor differs (moved %s)"), bMovedFirst ? TEXT("first") : TEXT("last")),
				Cambriana::Editor::DiffersFromStartingValue(*Handle));
		}
	}
	DestroyActor(Inline);
	DestroyActor(AtStart);
	DestroyActor(Moved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetRebuildsTest,
	"Cambriana.Editor.LiveActorDetails.ResettingASettingRebuildsTheActorWithTheStartingValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetRebuildsTest::RunTest(const FString& Parameters)
{
	const FName Sides = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides);
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(GrammarWithText(SidesStatingText));
	if (!TestNotNull(TEXT("An actor is placed from a grammar"), Actor))
	{
		return false;
	}
	Actor->bOverrideSides = true;
	Actor->Sides = 9;
	Actor->Rebuild();
	TestEqual(TEXT("The actor built 9 sides"), Actor->GetLastBuildInput().Settings.Sides, 9);
	const int32 BuildsBefore = Actor->GetBuildCount();

	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	const TSharedPtr<IPropertyHandle> Handle = HandleOf(*Rows, Sides);
	if (TestTrue(TEXT("Sides has a Details row"), Handle.IsValid()))
	{
		Cambriana::Editor::ResetToStartingValues(*Handle);
		TestEqual(TEXT("The reset sets the text's 3 sides"), Actor->Sides, 3);
		TestEqual(TEXT("The reset rebuilt the actor once"), Actor->GetBuildCount(), BuildsBefore + 1);
		TestEqual(TEXT("The rebuild used 3 sides"), Actor->GetLastBuildInput().Settings.Sides, 3);
	}
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorOverridesHiddenTest,
	"Cambriana.Editor.LiveActorDetails.TheInlineOverridesAreNotAlsoShownAsRowsOfTheirOwn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorOverridesHiddenTest::RunTest(const FString& Parameters)
{
	ACambrianaLiveActor* Actor = SpawnInlineActor(SidesLeavingText);
	if (!TestNotNull(TEXT("An inline actor spawns"), Actor))
	{
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	for (const FName Override : { GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideMinThickness), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideClosedForm),
			 GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideSides), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideSize) })
	{
		TestNull(FString::Printf(TEXT("%s has no row of its own"), *Override.ToString()), FindNode(Rows->GetRootTreeNodes(), Override).Get());
	}
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorSeedRowTest,
	"Cambriana.Editor.LiveActorDetails.TheSeedRowHasANewSeedButtonThatChangesTheActorsSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorSeedRowTest::RunTest(const FString& Parameters)
{
	ACambrianaLiveActor* Actor = SpawnInlineActor(SidesLeavingText);
	if (!TestNotNull(TEXT("An inline actor spawns"), Actor))
	{
		return false;
	}
	Actor->Seed = 1111;
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	const TSharedPtr<IDetailTreeNode> Node = FindNode(Rows->GetRootTreeNodes(), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Seed));
	const TSharedPtr<SWidget> Value = Node ? Node->CreateNodeWidgets().ValueWidget : nullptr;
	const TSharedPtr<SWidget> Button = Value ? FindWidgetOfType(Value.ToSharedRef(), TEXT("SButton")) : nullptr;
	if (TestTrue(TEXT("The seed row has a new-seed button"), Button.IsValid()))
	{
		StaticCastSharedPtr<SButton>(Button)->SimulateClick();
		TestNotEqual(TEXT("The button gives the actor a new seed"), Actor->Seed, 1111u);
	}
	DestroyActor(Actor);
	return true;
}

#endif
