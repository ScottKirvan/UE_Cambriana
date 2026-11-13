#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaLiveActor.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "IDetailPropertyRow.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::DestroyActor;
using Cambriana::Editor::Tests::FindNode;
using Cambriana::Editor::Tests::PlaceInCurrentLevel;
using Cambriana::Editor::Tests::SpawnInlineActor;

namespace CambrianaLiveActorResetTests
{
	const FName RecursionName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Recursion);
	const FName BasicAngleName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle);
	const FName ThicknessName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Thickness);
	const FName SizeModeName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode);
	const FName SizeValueName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue);
	const FName SidesName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides);
	const FName ClosedFormName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm);
	const FName MinThicknessName = GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, MinThickness);
	const FName AllSettings[] = { RecursionName, BasicAngleName, ThicknessName, SizeModeName, SizeValueName, SidesName, ClosedFormName, MinThicknessName };

	const TCHAR* StatingText = TEXT("generations 3\nangle 40\nsides 3\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	UCambrianaGrammar* CreateGrammar(const TCHAR* Text)
	{
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		UCambrianaGrammar* Grammar = Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass()), RF_Transactional, nullptr, nullptr));
		if (Grammar)
		{
			Grammar->GrammarText = Text;
		}
		return Grammar;
	}

	UCambrianaGrammar* CreateStatingGrammarWithAssetSettings()
	{
		UCambrianaGrammar* Grammar = CreateGrammar(StatingText);
		if (Grammar)
		{
			Grammar->SizeMode = ECambrianaSizeMode::Step;
			Grammar->SizeValue = 10.0;
			Grammar->Sides = 5;
			Grammar->bClosedForm = false;
			Grammar->MinThickness = 20.0;
		}
		return Grammar;
	}

	void MoveEverySettingAway(ACambrianaLiveActor& Actor, ECambrianaSizeMode SizeMode)
	{
		Actor.Recursion = 7.0;
		Actor.BasicAngle = 11.0;
		Actor.Thickness = 12.0;
		Actor.SizeMode = SizeMode;
		Actor.SizeValue = 99.0;
		Actor.Sides = 9;
		Actor.bClosedForm = !Actor.bClosedForm;
		Actor.MinThickness = 4.0;
	}

	FString DescribeSettings(const ACambrianaLiveActor& Actor)
	{
		return FString::Printf(TEXT("recursion %g, angle %g, thickness %g, size %s %g, sides %d, closed %s, min thickness %g"), Actor.Recursion,
			Actor.BasicAngle, Actor.Thickness, Actor.SizeMode == ECambrianaSizeMode::Fit ? TEXT("Fit") : TEXT("Step"), Actor.SizeValue, Actor.Sides,
			Actor.bClosedForm ? TEXT("yes") : TEXT("no"), Actor.MinThickness);
	}

	FString DescribeOverrides(const ACambrianaLiveActor& Actor)
	{
		return FString::Printf(TEXT("recursion %d, angle %d, thickness %d, size %d, sides %d, closed %d, min thickness %d"), Actor.bOverrideRecursion,
			Actor.bOverrideBasicAngle, Actor.bOverrideThickness, Actor.bOverrideSize, Actor.bOverrideSides, Actor.bOverrideClosedForm,
			Actor.bOverrideMinThickness);
	}

	/** A setting's Details row, with its reset-to-default arrow as the details panel draws and clicks it. */
	class FSettingRow
	{
	public:
		FSettingRow(const IPropertyRowGenerator& Rows, FName PropertyName)
		{
			const TSharedPtr<IDetailTreeNode> Node = FindNode(Rows.GetRootTreeNodes(), PropertyName, true);
			const TSharedPtr<IDetailPropertyRow> Row = Node ? Node->GetRow() : nullptr;
			if (Row)
			{
				Handle = Row->GetPropertyHandle();
				FDetailWidgetRow WidgetRow;
				TSharedPtr<SWidget> NameWidget;
				TSharedPtr<SWidget> ValueWidget;
				Row->GetDefaultWidgets(NameWidget, ValueWidget, WidgetRow);
				Reset = WidgetRow.GetCustomResetToDefault();
			}
		}

		bool IsValid() const { return Handle.IsValid(); }

		bool IsArrowShown() const
		{
			return Reset ? Reset->IsResetToDefaultVisible(Handle) : Handle->DiffersFromDefault();
		}

		void ClickArrow() const
		{
			if (Reset)
			{
				Reset->OnResetToDefaultClicked(Handle);
			}
			else
			{
				Handle->ResetToDefault();
			}
		}

	private:
		TSharedPtr<IPropertyHandle> Handle;
		TOptional<FResetToDefaultOverride> Reset;
	};

	TSharedRef<IPropertyRowGenerator> MakeRows(const TArray<UObject*>& Objects)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		const TSharedRef<IPropertyRowGenerator> Rows = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Rows->SetObjects(Objects);
		return Rows;
	}

	void ResetEverySetting(FAutomationTestBase& Test, const IPropertyRowGenerator& Rows)
	{
		for (const FName Name : AllSettings)
		{
			const FSettingRow Row(Rows, Name);
			if (!Test.TestTrue(FString::Printf(TEXT("%s has a Details row"), *Name.ToString()), Row.IsValid()))
			{
				continue;
			}
			Test.TestTrue(FString::Printf(TEXT("%s's arrow shows while the value differs"), *Name.ToString()), Row.IsArrowShown());
			Row.ClickArrow();
			Test.TestFalse(FString::Printf(TEXT("%s's arrow hides after the reset"), *Name.ToString()), Row.IsArrowShown());
		}
	}

}

using namespace CambrianaLiveActorResetTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetFromAssetTest,
	"Cambriana.Editor.LiveActorReset.EachSettingResetsToTheTextThenTheAssetThenTheDefaultAndKeepsItsOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetFromAssetTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateStatingGrammarWithAssetSettings();
	ACambrianaLiveActor* Actor = Grammar ? PlaceInCurrentLevel(Grammar) : nullptr;
	if (!TestNotNull(TEXT("A live actor was placed from the grammar"), Actor))
	{
		return false;
	}
	MoveEverySettingAway(*Actor, ECambrianaSizeMode::Fit);
	Actor->bOverrideRecursion = true;
	Actor->bOverrideSize = true;
	Actor->bOverrideSides = true;
	const FString Overrides = DescribeOverrides(*Actor);

	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	ResetEverySetting(*this, *Rows);

	TestEqual(TEXT("Recursion, angle and sides are the text's; size, closed form and min thickness the asset's; thickness the default"),
		DescribeSettings(*Actor), FString(TEXT("recursion 3, angle 40, thickness 50, size Step 10, sides 3, closed no, min thickness 20")));
	TestEqual(TEXT("Every override checkbox is as it was"), DescribeOverrides(*Actor), Overrides);
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetAtResetTimeTest,
	"Cambriana.Editor.LiveActorReset.ResetUsesTheTextAndAssetAsTheyAreAtTheReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetAtResetTimeTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateStatingGrammarWithAssetSettings();
	ACambrianaLiveActor* Actor = Grammar ? PlaceInCurrentLevel(Grammar) : nullptr;
	if (!TestNotNull(TEXT("A live actor was placed from the grammar"), Actor))
	{
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	for (const FName Name : { RecursionName, SizeValueName, MinThicknessName })
	{
		const FSettingRow Row(*Rows, Name);
		if (TestTrue(FString::Printf(TEXT("%s has a Details row"), *Name.ToString()), Row.IsValid()))
		{
			TestFalse(FString::Printf(TEXT("%s's arrow is hidden before the edit: the actor has its placement value"), *Name.ToString()), Row.IsArrowShown());
		}
	}

	Grammar->GrammarText = TEXT("generations 5\nangle 40\nsides 3\naxiom A\n\ngrow {\n    A -> F A\n}\n");
	Grammar->SizeValue = 15.0;
	Grammar->MinThickness = 30.0;

	for (const FName Name : { RecursionName, SizeValueName, MinThicknessName })
	{
		const FSettingRow Row(*Rows, Name);
		if (TestTrue(FString::Printf(TEXT("%s has a Details row"), *Name.ToString()), Row.IsValid()))
		{
			TestTrue(FString::Printf(TEXT("%s's arrow shows: the placement value differs from the current text or asset"), *Name.ToString()),
				Row.IsArrowShown());
			Row.ClickArrow();
		}
	}
	TestEqual(TEXT("Recursion is the edited text's"), Actor->Recursion, 5.0);
	TestEqual(TEXT("Size value is the edited asset's"), Actor->SizeValue, 15.0);
	TestEqual(TEXT("Min thickness is the edited asset's"), Actor->MinThickness, 30.0);
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetArrowShownTest,
	"Cambriana.Editor.LiveActorReset.ArrowShowsOnlyWhereTheValueDiffersFromItsResetValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetArrowShownTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateStatingGrammarWithAssetSettings();
	ACambrianaLiveActor* Actor = Grammar ? PlaceInCurrentLevel(Grammar) : nullptr;
	if (!TestNotNull(TEXT("A live actor was placed from the grammar"), Actor))
	{
		return false;
	}
	Actor->Sides = 9;

	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	for (const FName Name : AllSettings)
	{
		const FSettingRow Row(*Rows, Name);
		if (TestTrue(FString::Printf(TEXT("%s has a Details row"), *Name.ToString()), Row.IsValid()))
		{
			TestEqual(FString::Printf(TEXT("%s's arrow shows only if it is the changed setting"), *Name.ToString()), Row.IsArrowShown(), Name == SidesName);
		}
	}
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetInlineTest,
	"Cambriana.Editor.LiveActorReset.InlineTextResetsToTheTextThenTheDefaultWithTheCoreDefaultSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetInlineTest::RunTest(const FString& Parameters)
{
	ACambrianaLiveActor* Actor = SpawnInlineActor(TEXT("generations 3\nsides 4\nminthickness 2cm\naxiom A\n\ngrow {\n    A -> F A\n}\n"));
	if (!TestNotNull(TEXT("A live actor with inline text was spawned"), Actor))
	{
		return false;
	}
	MoveEverySettingAway(*Actor, ECambrianaSizeMode::Step);
	Actor->bOverrideBasicAngle = true;
	Actor->bOverrideMinThickness = true;
	const FString Overrides = DescribeOverrides(*Actor);

	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	ResetEverySetting(*this, *Rows);

	TestEqual(TEXT("Recursion, sides and min thickness are the text's; the rest the defaults, with the core's default size Fit 200 cm"),
		DescribeSettings(*Actor), FString(TEXT("recursion 3, angle 25, thickness 50, size Fit 200, sides 4, closed yes, min thickness 2")));
	TestEqual(TEXT("Every override checkbox is as it was"), DescribeOverrides(*Actor), Overrides);
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetUndoTest,
	"Cambriana.Editor.LiveActorReset.ResetIsUndoable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetUndoTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateStatingGrammarWithAssetSettings();
	ACambrianaLiveActor* Actor = Grammar ? PlaceInCurrentLevel(Grammar) : nullptr;
	if (!TestNotNull(TEXT("A live actor was placed from the grammar"), Actor))
	{
		return false;
	}
	Actor->Sides = 9;
	Actor->MinThickness = 4.0;

	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Actor });
	const FSettingRow Row(*Rows, SidesName);
	if (TestTrue(TEXT("Sides has a Details row"), Row.IsValid()))
	{
		Row.ClickArrow();
		TestEqual(TEXT("The reset sets the text's sides"), Actor->Sides, 3);
		TestTrue(TEXT("The reset can be undone"), GEditor->UndoTransaction());
		TestEqual(TEXT("Undo restores the actor's sides"), Actor->Sides, 9);
		TestEqual(TEXT("Undo leaves the other settings"), Actor->MinThickness, 4.0);
		TestTrue(TEXT("The reset can be redone"), GEditor->RedoTransaction());
		TestEqual(TEXT("Redo resets the sides again"), Actor->Sides, 3);
	}
	DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorResetSeveralTest,
	"Cambriana.Editor.LiveActorReset.SeveralSelectedActorsEachResetToTheirOwnValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorResetSeveralTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Three = CreateGrammar(TEXT("sides 3\naxiom A\n\ngrow {\n    A -> F A\n}\n"));
	UCambrianaGrammar* Six = CreateGrammar(TEXT("sides 6\naxiom A\n\ngrow {\n    A -> F A\n}\n"));
	ACambrianaLiveActor* First = Three ? PlaceInCurrentLevel(Three) : nullptr;
	ACambrianaLiveActor* Second = Six ? PlaceInCurrentLevel(Six) : nullptr;
	if (!TestNotNull(TEXT("First actor placed"), First) || !TestNotNull(TEXT("Second actor placed"), Second))
	{
		DestroyActor(First);
		DestroyActor(Second);
		return false;
	}
	Second->Sides = 9;

	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ First, Second });
	const FSettingRow Row(*Rows, SidesName);
	if (TestTrue(TEXT("Sides has a Details row"), Row.IsValid()))
	{
		TestTrue(TEXT("The arrow shows when any selected actor differs"), Row.IsArrowShown());
		First->Sides = 9;
		Row.ClickArrow();
		TestEqual(TEXT("The first actor takes its text's sides"), First->Sides, 3);
		TestEqual(TEXT("The second actor takes its text's sides"), Second->Sides, 6);
		TestFalse(TEXT("The arrow hides when every actor is at its value"), Row.IsArrowShown());
	}
	DestroyActor(First);
	DestroyActor(Second);
	return true;
}

#endif
