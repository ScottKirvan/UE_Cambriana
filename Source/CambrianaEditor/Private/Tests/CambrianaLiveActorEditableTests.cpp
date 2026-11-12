#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "Editor.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SCheckBox.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CreateGrammarWithFactory;

namespace CambrianaLiveActorEditableTests
{
	struct FSetting
	{
		FName Value;
		/** NAME_None for a value whose checkbox is shown on another row. */
		FName ShownToggle;
		FName Toggle;
	};

	const FSetting Settings[] = {
		{ TEXT("Recursion"), TEXT("bOverrideRecursion"), TEXT("bOverrideRecursion") },
		{ TEXT("BasicAngle"), TEXT("bOverrideBasicAngle"), TEXT("bOverrideBasicAngle") },
		{ TEXT("Thickness"), TEXT("bOverrideThickness"), TEXT("bOverrideThickness") },
		{ TEXT("MinThickness"), TEXT("bOverrideMinThickness"), TEXT("bOverrideMinThickness") },
		{ TEXT("bClosedForm"), TEXT("bOverrideClosedForm"), TEXT("bOverrideClosedForm") },
		{ TEXT("Sides"), TEXT("bOverrideSides"), TEXT("bOverrideSides") },
		{ TEXT("SizeMode"), TEXT("bOverrideSize"), TEXT("bOverrideSize") },
		{ TEXT("SizeValue"), NAME_None, TEXT("bOverrideSize") },
	};

	const TCHAR* LegacyText = TEXT("2\n25\n50\nA\nA=F^'A\n@\n");
	const TCHAR* LsysStatingSides = TEXT("sides 3\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	bool& OverrideOf(ACambrianaLiveActor& Actor, FName Toggle)
	{
		FBoolProperty* Property = CastFieldChecked<FBoolProperty>(ACambrianaLiveActor::StaticClass()->FindPropertyByName(Toggle));
		return *Property->ContainerPtrToValuePtr<bool>(&Actor);
	}

	void SetEveryOverride(ACambrianaLiveActor& Actor, bool bOn)
	{
		for (const FSetting& Setting : Settings)
		{
			OverrideOf(Actor, Setting.Toggle) = bOn;
		}
	}

	TSharedPtr<IDetailTreeNode> FindNode(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, FName PropertyName)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() && Handle->GetProperty()->GetFName() == PropertyName)
			{
				return Node;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, false);
			if (TSharedPtr<IDetailTreeNode> Found = FindNode(Children, PropertyName))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** Whether no widget in the tree is disabled; a disabled widget disables everything inside it. */
	bool IsWholeTreeEnabled(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid())
		{
			return false;
		}
		if (!Widget->IsEnabled())
		{
			return false;
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			if (!IsWholeTreeEnabled(Children->GetChildAt(Index)))
			{
				return false;
			}
		}
		return true;
	}

	/** The visible checkboxes in the tree, each with whether it and every widget around it inside the tree is enabled. */
	void FindVisibleCheckBoxes(const TSharedRef<SWidget>& Widget, bool bEnabled, TArray<TPair<TSharedRef<SCheckBox>, bool>>& Out)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		bEnabled = bEnabled && Widget->IsEnabled();
		if (Widget->GetType() == TEXT("SCheckBox"))
		{
			Out.Emplace(StaticCastSharedRef<SCheckBox>(Widget), bEnabled);
			return;
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			FindVisibleCheckBoxes(Children->GetChildAt(Index), bEnabled, Out);
		}
	}

	TSharedPtr<SCheckBox> FindCheckBox(const TSharedRef<SWidget>& Widget)
	{
		if (Widget->GetType() == TEXT("SCheckBox"))
		{
			return StaticCastSharedRef<SCheckBox>(Widget);
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SCheckBox> Found = FindCheckBox(Children->GetChildAt(Index)))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** A setting's Details row as the details panel builds its widgets. */
	struct FSettingRow
	{
		FNodeWidgets Widgets;
		bool bHasEditCondition = false;
		bool bFound = false;

		FSettingRow(const IPropertyRowGenerator& Rows, FName PropertyName)
		{
			if (const TSharedPtr<IDetailTreeNode> Node = FindNode(Rows.GetRootTreeNodes(), PropertyName))
			{
				Widgets = Node->CreateNodeWidgets();
				const FProperty* Property = ACambrianaLiveActor::StaticClass()->FindPropertyByName(PropertyName);
				bHasEditCondition = Property && Property->HasMetaData(TEXT("EditCondition"));
				bFound = true;
			}
		}

		/**
		 * As SDetailSingleItemRow enables a row: its name and value widgets, and its edit condition when it has one. The edit condition
		 * widget's checkbox shows the condition's value whether or not the checkbox is visible.
		 */
		bool IsValueEditable() const
		{
			const TSharedPtr<SCheckBox> Condition = Widgets.EditConditionWidget ? FindCheckBox(Widgets.EditConditionWidget.ToSharedRef()) : nullptr;
			const bool bConditionMet = !bHasEditCondition || (Condition && Condition->IsChecked());
			return bConditionMet && IsWholeTreeEnabled(Widgets.ValueWidget) && IsWholeTreeEnabled(Widgets.NameWidget);
		}

		/** The row's checkboxes left of its value: the edit condition's or the name column's. */
		TArray<TPair<TSharedRef<SCheckBox>, bool>> CheckBoxes() const
		{
			TArray<TPair<TSharedRef<SCheckBox>, bool>> Found;
			for (const TSharedPtr<SWidget>& Column : { Widgets.EditConditionWidget, Widgets.NameWidget })
			{
				if (Column)
				{
					FindVisibleCheckBoxes(Column.ToSharedRef(), true, Found);
				}
			}
			return Found;
		}
	};

	TSharedRef<IPropertyRowGenerator> MakeRows(ACambrianaLiveActor& Actor)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		const TSharedRef<IPropertyRowGenerator> Rows = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Rows->SetObjects({ &Actor });
		return Rows;
	}

	ACambrianaLiveActor* SpawnActor(UCambrianaGrammar* Grammar, const TCHAR* InlineText)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		ACambrianaLiveActor* Actor = World ? World->SpawnActor<ACambrianaLiveActor>() : nullptr;
		if (Actor)
		{
			Actor->Grammar = Grammar;
			Actor->InlineGrammar = InlineText;
		}
		return Actor;
	}

	void Destroy(ACambrianaLiveActor* Actor)
	{
		if (Actor)
		{
			Actor->GetWorld()->EditorDestroyActor(Actor, false);
		}
	}

	struct FEditableCase
	{
		const TCHAR* Label;
		const TCHAR* AssetText;
		const TCHAR* InlineText;
		bool bOverridesOn;
		TArray<FName> Editable;
	};
}

using namespace CambrianaLiveActorEditableTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDetailsEditableTest,
	"Cambriana.Editor.LiveActorDetails.EachSettingIsEditableExactlyWhenTheBuildUsesTheActorsValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDetailsEditableTest::RunTest(const FString& Parameters)
{
	// SDD-unreal.md, Live actor: an override on uses the actor's value. With it off, recursion, basic angle and thickness come from the
	// text; min thickness, closed form and sides from the text when it states them, else the asset, else the actor without an asset;
	// size from the asset, else, for inline .ls text, the actor, else (inline .lsys) the text or the core's default.
	const TArray<FEditableCase> Cases = {
		{ TEXT("inline .ls text, overrides off"), nullptr, LegacyText, false, { TEXT("MinThickness"), TEXT("bClosedForm"), TEXT("Sides"), TEXT("SizeMode"), TEXT("SizeValue") } },
		{ TEXT("inline .lsys text stating sides, overrides off"), nullptr, LsysStatingSides, false, { TEXT("MinThickness"), TEXT("bClosedForm") } },
		{ TEXT("an asset holding .lsys text, overrides off"), LsysStatingSides, TEXT(""), false, {} },
		{ TEXT("an asset holding .ls text, overrides off"), LegacyText, TEXT(""), false, {} },
		{ TEXT("an asset holding .lsys text, overrides on"), LsysStatingSides, TEXT(""), true,
			{ TEXT("Recursion"), TEXT("BasicAngle"), TEXT("Thickness"), TEXT("MinThickness"), TEXT("bClosedForm"), TEXT("Sides"), TEXT("SizeMode"), TEXT("SizeValue") } },
		{ TEXT("inline .lsys text stating sides, overrides on"), nullptr, LsysStatingSides, true,
			{ TEXT("Recursion"), TEXT("BasicAngle"), TEXT("Thickness"), TEXT("MinThickness"), TEXT("bClosedForm"), TEXT("Sides"), TEXT("SizeMode"), TEXT("SizeValue") } },
	};

	for (const FEditableCase& Case : Cases)
	{
		UCambrianaGrammar* Grammar = nullptr;
		if (Case.AssetText)
		{
			Grammar = CreateGrammarWithFactory();
			Grammar->GrammarText = Case.AssetText;
		}
		ACambrianaLiveActor* Actor = SpawnActor(Grammar, Case.InlineText);
		if (!TestNotNull(FString::Printf(TEXT("%s: a live actor spawns"), Case.Label), Actor))
		{
			continue;
		}
		SetEveryOverride(*Actor, Case.bOverridesOn);
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows(*Actor);
		for (const FSetting& Setting : Settings)
		{
			const FSettingRow Row(*Rows, Setting.Value);
			if (TestTrue(FString::Printf(TEXT("%s: %s has a Details row"), Case.Label, *Setting.Value.ToString()), Row.bFound))
			{
				TestEqual(FString::Printf(TEXT("%s: %s is editable"), Case.Label, *Setting.Value.ToString()), Row.IsValueEditable(),
					Case.Editable.Contains(Setting.Value));
			}
		}
		Destroy(Actor);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorDetailsCheckboxTest,
	"Cambriana.Editor.LiveActorDetails.EachOverrideCheckboxIsInlineEditableAndTogglesItsOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorDetailsCheckboxTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* InlineText : { LegacyText, LsysStatingSides })
	{
		ACambrianaLiveActor* Actor = SpawnActor(nullptr, InlineText);
		if (!TestNotNull(TEXT("A live actor spawns"), Actor))
		{
			continue;
		}
		const TSharedRef<IPropertyRowGenerator> Rows = MakeRows(*Actor);
		for (const FSetting& Setting : Settings)
		{
			const FString Label = FString::Printf(TEXT("%s, %s"), InlineText == LegacyText ? TEXT(".ls") : TEXT(".lsys"), *Setting.Value.ToString());
			const FSettingRow Row(*Rows, Setting.Value);
			if (!TestTrue(Label + TEXT(" has a Details row"), Row.bFound))
			{
				continue;
			}
			const TArray<TPair<TSharedRef<SCheckBox>, bool>> CheckBoxes = Row.CheckBoxes();
			if (Setting.ShownToggle.IsNone())
			{
				TestEqual(Label + TEXT(" shows no checkbox; its override's is on another row"), CheckBoxes.Num(), 0);
				continue;
			}
			if (!TestEqual(Label + TEXT(" shows one checkbox left of its value"), CheckBoxes.Num(), 1))
			{
				continue;
			}
			const TSharedRef<SCheckBox> Box = CheckBoxes[0].Key;
			TestTrue(Label + TEXT("'s checkbox is editable"), CheckBoxes[0].Value);
			TestFalse(Label + TEXT("'s checkbox shows the override off"), Box->IsChecked());
			Box->ToggleCheckedState();
			TestTrue(Label + TEXT("'s checkbox turns the override on"), OverrideOf(*Actor, Setting.Toggle));
			TestTrue(Label + TEXT("'s checkbox shows the override on"), Box->IsChecked());
			Box->ToggleCheckedState();
			TestFalse(Label + TEXT("'s checkbox turns the override off again"), OverrideOf(*Actor, Setting.Toggle));
		}
		Destroy(Actor);
	}
	return true;
}

#endif
