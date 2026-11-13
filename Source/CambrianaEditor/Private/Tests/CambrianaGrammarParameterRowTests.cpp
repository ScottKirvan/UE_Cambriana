#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarCustomization.h"
#include "Editor.h"
#include "IDetailTreeNode.h"
#include "IDetailsView.h"
#include "IPropertyRowGenerator.h"
#include "Input/Events.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/STextBlock.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::FindStoredValue;
using Cambriana::Editor::Tests::FindWidgetOfType;

namespace
{
	const FName PoseCategory(TEXT("Pose Parameters"));
	const FName ShapeCategory(TEXT("Shape Parameters"));
	const FName StructureCategory(TEXT("Structure Parameters"));
	const FName UntieredCategory(TEXT("Parameters"));

	// One parameter of each tier, in file order; the axiom is one chain of `count` segments.
	const TCHAR* const FourTierText = TEXT("pose param bend = 10\nshape param girth = 1.5\nstructure param count = 3\nparam twice = count * 2\n\naxiom repeat count { F }\n");

	/** A details view that has generated its rows, as a window would draw it. */
	class FRenderedDetails
	{
	public:
		explicit FRenderedDetails(UObject* Object)
		{
			FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			FDetailsViewArgs Args;
			Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			View = PropertyEditor.CreateDetailView(Args);
			View->SetObject(Object, true);
			const TSharedRef<SWidget> Root = View.ToSharedRef();
			const FGeometry Geometry = FGeometry::MakeRoot(FVector2f(600.0f, 1200.0f), FSlateLayoutTransform());
			for (int32 Pass = 0; Pass < 12; ++Pass)
			{
				Root->Tick(Geometry, 0.0, 0.016f);
				TArray<TSharedRef<SWidget>> Pending = { Root };
				while (!Pending.IsEmpty())
				{
					const TSharedRef<SWidget> Widget = Pending.Pop();
					if (Widget->GetType() == TEXT("SDetailTree"))
					{
						Widget->Tick(Geometry, 0.0, 0.016f);
					}
					FChildren* Children = Widget->GetChildren();
					for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
					{
						Pending.Add(Children->GetChildAt(Index));
					}
				}
				Root->SlatePrepass(1.0f);
			}
		}

		/** The row whose name text is Label. */
		TSharedPtr<SWidget> FindRow(const TCHAR* Label) const
		{
			TArray<TSharedRef<SWidget>> Pending = { View.ToSharedRef() };
			while (!Pending.IsEmpty())
			{
				const TSharedRef<SWidget> Widget = Pending.Pop();
				if (Widget->GetType() == TEXT("SDetailSingleItemRow"))
				{
					const TSharedPtr<SWidget> Name = FindWidgetOfType(Widget, TEXT("STextBlock"));
					if (Name && StaticCastSharedPtr<STextBlock>(Name)->GetText().ToString() == Label)
					{
						return Widget;
					}
				}
				FChildren* Children = Widget->GetChildren();
				for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
				{
					Pending.Add(Children->GetChildAt(Index));
				}
			}
			return nullptr;
		}

	private:
		TSharedPtr<IDetailsView> View;
	};

	/** Types Text into the row's value field and presses Enter, as a user commits a value. */
	bool CommitValue(const TSharedRef<SWidget>& Row, const TCHAR* Text)
	{
		const TSharedPtr<SWidget> Field = FindWidgetOfType(Row, TEXT("SEditableText"));
		if (!Field)
		{
			return false;
		}
		SEditableText& Edit = static_cast<SEditableText&>(*Field);
		Edit.SetText(FText::FromString(Text));
		const FKeyEvent Enter(EKeys::Enter, FModifierKeysState(), 0, false, 13, 0);
		static_cast<SWidget&>(Edit).OnKeyDown(FGeometry(), Enter);
		return true;
	}

	/** The reset arrow of the row: the first button of its right-hand toolbar. */
	TSharedPtr<SButton> ResetButtonOf(const TSharedRef<SWidget>& Row)
	{
		const TSharedPtr<SWidget> Toolbar = FindWidgetOfType(Row, TEXT("SMultiBoxWidget"));
		const TSharedPtr<SWidget> Button = Toolbar ? FindWidgetOfType(Toolbar.ToSharedRef(), TEXT("SButton")) : nullptr;
		if (Button)
		{
			Button->SlatePrepass(1.0f);
		}
		return StaticCastSharedPtr<SButton>(Button);
	}

	TSharedRef<IPropertyRowGenerator> DetailsOf(UCambrianaGrammar* Grammar)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		const TSharedRef<IPropertyRowGenerator> Rows = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Rows->SetObjects({ Grammar });
		return Rows;
	}

	TSharedPtr<IDetailTreeNode> FindCategory(const IPropertyRowGenerator& Rows, FName Category)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Rows.GetRootTreeNodes())
		{
			if (Node->GetNodeType() == EDetailNodeType::Category && Node->GetNodeName() == Category)
			{
				return Node;
			}
		}
		return nullptr;
	}

	/** The rows of one parameter category, in row order. */
	TArray<TSharedRef<IDetailTreeNode>> RowsOf(const IPropertyRowGenerator& Rows, FName Category)
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		if (const TSharedPtr<IDetailTreeNode> Node = FindCategory(Rows, Category))
		{
			Node->GetChildren(Children);
		}
		return Children;
	}

	/** The number a row's value field shows; NaN, which equals nothing, without a numeric field. */
	double ShownValue(const TSharedRef<IDetailTreeNode>& Row)
	{
		const TSharedPtr<SWidget> Value = Row->CreateNodeWidgets().ValueWidget;
		if (Value)
		{
			// Bound Slate attributes refresh in the prepass of each frame, which a widget outside a window never gets.
			Value->SlatePrepass(1.0f);
		}
		const TSharedPtr<SWidget> Field = Value ? FindWidgetOfType(Value.ToSharedRef(), TEXT("SEditableText")) : nullptr;
		const FString Text = Field ? StaticCastSharedPtr<SEditableText>(Field)->GetText().ToString() : FString();
		return Text.IsNumeric() ? FCString::Atod(*Text) : std::numeric_limits<double>::quiet_NaN();
	}

	double ShownValue(const IPropertyRowGenerator& Rows, FName Category, int32 Index)
	{
		const TArray<TSharedRef<IDetailTreeNode>> Children = RowsOf(Rows, Category);
		return Children.IsValidIndex(Index) ? ShownValue(Children[Index]) : std::numeric_limits<double>::quiet_NaN();
	}

	void SetGrammarText(UCambrianaGrammar* Grammar, const TCHAR* Text)
	{
		FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText));
		Grammar->PreEditChange(Property);
		Grammar->GrammarText = Text;
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Grammar->PostEditChangeProperty(Event);
	}

	/** Counts the times the generator rebuilds its rows. */
	class FRefreshCounter
	{
	public:
		explicit FRefreshCounter(IPropertyRowGenerator& InRows)
			: Rows(InRows)
		{
			Handle = Rows.OnRowsRefreshed().AddLambda([this] { ++Count; });
		}

		~FRefreshCounter() { Rows.OnRowsRefreshed().Remove(Handle); }

		FRefreshCounter(const FRefreshCounter&) = delete;
		FRefreshCounter& operator=(const FRefreshCounter&) = delete;

		int32 Count = 0;

	private:
		IPropertyRowGenerator& Rows;
		FDelegateHandle Handle;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParameterRowCommitTest,
	"Cambriana.Editor.GrammarParameterRows.CommittingAValueInARowStoresItInTheGrammarAndCommittingAgainReplacesIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaParameterRowCommitTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = FourTierText;
	{
		const FRenderedDetails Details(Grammar);
		const TSharedPtr<SWidget> Row = Details.FindRow(TEXT("count"));
		if (!TestTrue(TEXT("Details has a row for count"), Row.IsValid()) || !TestTrue(TEXT("The row has a value field"), CommitValue(Row.ToSharedRef(), TEXT("7"))))
		{
			return false;
		}
	}
	const FCambrianaParameterValue* Stored = FindStoredValue(*Grammar, TEXT("count"));
	TestTrue(TEXT("Committing 7 stores count = 7"), Stored && Stored->Value == 7.0);

	{
		const FRenderedDetails Details(Grammar);
		const TSharedPtr<SWidget> Row = Details.FindRow(TEXT("count"));
		if (!TestTrue(TEXT("Details has a row for count again"), Row.IsValid()) || !TestTrue(TEXT("The row has a value field"), CommitValue(Row.ToSharedRef(), TEXT("9"))))
		{
			return false;
		}
	}
	Stored = FindStoredValue(*Grammar, TEXT("count"));
	TestTrue(TEXT("Committing 9 stores count = 9"), Stored && Stored->Value == 9.0);
	TestEqual(TEXT("There is still one stored value"), Grammar->ParameterValues.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParameterRowResetTest,
	"Cambriana.Editor.GrammarParameterRows.TheResetArrowShowsOnlyForAStoredValueAndRemovesIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaParameterRowResetTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = FourTierText;
	Cambriana::Editor::SetParameterValue(*Grammar, TEXT("count"), 5.0);
	{
		const FRenderedDetails Details(Grammar);
		const TSharedPtr<SWidget> Stored = Details.FindRow(TEXT("count"));
		const TSharedPtr<SWidget> Unstored = Details.FindRow(TEXT("bend"));
		if (!TestTrue(TEXT("Details has rows for count and bend"), Stored.IsValid() && Unstored.IsValid()))
		{
			return false;
		}
		const TSharedPtr<SButton> StoredArrow = ResetButtonOf(Stored.ToSharedRef());
		const TSharedPtr<SButton> UnstoredArrow = ResetButtonOf(Unstored.ToSharedRef());
		if (!TestTrue(TEXT("Both rows have a reset arrow"), StoredArrow.IsValid() && UnstoredArrow.IsValid()))
		{
			return false;
		}
		TestTrue(TEXT("The arrow is enabled for the stored value"), StoredArrow->IsEnabled());
		TestFalse(TEXT("The arrow is disabled for a value that is not stored"), UnstoredArrow->IsEnabled());
		StoredArrow->SimulateClick();
	}
	TestNull(TEXT("The arrow removes the stored value"), FindStoredValue(*Grammar, TEXT("count")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParameterCategoryOrderTest,
	"Cambriana.Editor.GrammarParameterRows.TierCategoriesAreInPoseShapeStructureThenUntieredOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaParameterCategoryOrderTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = FourTierText;
	const TSharedRef<IPropertyRowGenerator> Rows = DetailsOf(Grammar);
	TArray<FName> Shown;
	for (const TSharedRef<IDetailTreeNode>& Node : Rows->GetRootTreeNodes())
	{
		if (Node->GetNodeName() == PoseCategory || Node->GetNodeName() == ShapeCategory || Node->GetNodeName() == StructureCategory
			|| Node->GetNodeName() == UntieredCategory)
		{
			Shown.Add(Node->GetNodeName());
		}
	}
	TestTrue(TEXT("Pose, Shape, Structure, then the untiered parameters"), Shown == TArray<FName>({ PoseCategory, ShapeCategory, StructureCategory, UntieredCategory }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParameterRowsRefreshTest,
	"Cambriana.Editor.GrammarParameterRows.RowsRebuildWhenAParametersNameOrTierChangesAndNotForOtherChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaParameterRowsRefreshTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TEXT("pose param a = 1\nstructure param count = 3\n\naxiom repeat count { F }\n");
	UCambrianaGrammar* Other = CreateGrammarWithFactory();
	Other->GrammarText = TEXT("structure param other = 1\n\naxiom F\n");
	const TSharedRef<IPropertyRowGenerator> Rows = DetailsOf(Grammar);
	FRefreshCounter Refreshes(*Rows);

	Cambriana::Editor::SetParameterValue(*Grammar, TEXT("count"), 5.0);
	TestEqual(TEXT("Storing a value does not rebuild the rows"), Refreshes.Count, 0);
	SetGrammarText(Other, TEXT("structure param other = 2\n\naxiom F\n"));
	TestEqual(TEXT("A change to another grammar does not rebuild the rows"), Refreshes.Count, 0);

	SetGrammarText(Grammar, TEXT("shape param a = 1\nstructure param count = 3\n\naxiom repeat count { F }\n"));
	TestEqual(TEXT("A parameter changing tier rebuilds the rows once"), Refreshes.Count, 1);
	TestEqual(TEXT("and a is shown under Shape Parameters"), RowsOf(*Rows, ShapeCategory).Num(), 1);
	TestEqual(TEXT("and no longer under Pose Parameters"), RowsOf(*Rows, PoseCategory).Num(), 0);

	Grammar->ParameterValues.Reset();
	Cambriana::Editor::SetParameterValue(*Grammar, TEXT("count"), 5.0);
	SetGrammarText(Grammar, TEXT("shape param a = 1\nstructure param Count = 3\n\naxiom repeat Count { F }\n"));
	TestEqual(TEXT("A parameter renamed only in case rebuilds the rows"), Refreshes.Count, 2);
	TestEqual(TEXT("and Count shows the text's value, not the value stored for count"), ShownValue(*Rows, StructureCategory, 0), 3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParameterRowsCaseTest,
	"Cambriana.Editor.GrammarParameterRows.ParametersNamedOnlyInCaseEachShowTheirOwnValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaParameterRowsCaseTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TEXT("structure param Count = 7\nstructure param count = 3\n\naxiom repeat count { F }\n");
	const TSharedRef<IPropertyRowGenerator> Rows = DetailsOf(Grammar);
	if (!TestEqual(TEXT("Both parameters have a row"), RowsOf(*Rows, StructureCategory).Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Count shows its 7"), ShownValue(*Rows, StructureCategory, 0), 7.0);
	TestEqual(TEXT("count shows its 3"), ShownValue(*Rows, StructureCategory, 1), 3.0);
	return true;
}

#endif
