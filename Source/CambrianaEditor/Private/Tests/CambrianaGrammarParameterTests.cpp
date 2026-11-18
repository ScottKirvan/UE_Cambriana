#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarCustomization.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarPreview.h"
#include "Components/DynamicMeshComponent.h"
#include "Editor.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Editor::Tests::CloseGrammarEditor;
using Cambriana::Editor::Tests::CreateGrammarWithFactory;
using Cambriana::Editor::Tests::FindPropertyHandle;
using Cambriana::Editor::Tests::FindStoredValue;
using Cambriana::Editor::Tests::FindWidgetOfType;
using Cambriana::Editor::Tests::OpenGrammarEditor;
using Cambriana::Editor::Tests::PreviewVertexCount;
using Cambriana::Editor::Tests::TubeVertices;
using Cambriana::Editor::Tests::FindCategory;
using Cambriana::Editor::Tests::MakeRows;

namespace
{
	// One parameter of each tier, in file order; the axiom is one chain of `count` segments.
	FString ParameterGrammar(int32 Count = 3, const TCHAR* CountName = TEXT("count"), const TCHAR* Extra = TEXT(""))
	{
		return FString::Printf(TEXT("pose param bend = 10\nshape param girth = 1.5\nstructure param %s = %d\n%sparam twice = %s * 2\n\naxiom repeat %s { F }\n"),
			CountName, Count, Extra, CountName, CountName);
	}

	const FName PoseCategory(TEXT("Pose Parameters"));
	const FName ShapeCategory(TEXT("Shape Parameters"));
	const FName StructureCategory(TEXT("Structure Parameters"));
	const FName UntieredCategory(TEXT("Parameters"));
	const FName ParameterCategories[] = { PoseCategory, ShapeCategory, StructureCategory, UntieredCategory };

	/** Row names of each parameter category shown, in row order. */
	TMap<FName, TArray<FString>> ParameterRows(const IPropertyRowGenerator& Rows)
	{
		TMap<FName, TArray<FString>> Shown;
		for (const FName Category : ParameterCategories)
		{
			if (const TSharedPtr<IDetailTreeNode> Node = FindCategory(Rows, Category))
			{
				TArray<TSharedRef<IDetailTreeNode>> Children;
				Node->GetChildren(Children);
				TArray<FString>& Names = Shown.Add(Category);
				for (const TSharedRef<IDetailTreeNode>& Child : Children)
				{
					Names.Add(Child->GetNodeName().ToString());
				}
			}
		}
		return Shown;
	}

	TSharedPtr<IDetailTreeNode> FindParameterRow(const IPropertyRowGenerator& Rows, const TCHAR* Name)
	{
		for (const FName Category : ParameterCategories)
		{
			if (const TSharedPtr<IDetailTreeNode> Node = FindCategory(Rows, Category))
			{
				TArray<TSharedRef<IDetailTreeNode>> Children;
				Node->GetChildren(Children);
				for (const TSharedRef<IDetailTreeNode>& Child : Children)
				{
					if (Child->GetNodeName() == FName(Name))
					{
						return Child;
					}
				}
			}
		}
		return nullptr;
	}

	/** The number a parameter row's value field shows; NaN, which equals nothing, without the row or a numeric field. */
	double ShownValue(const IPropertyRowGenerator& Rows, const TCHAR* Name)
	{
		return Cambriana::Editor::Tests::ShownValue(FindParameterRow(Rows, Name));
	}

	FCambrianaGrammarEditorToolkit* OpenWithText(UCambrianaGrammar* Grammar, const FString& Text)
	{
		Grammar->GrammarText = Text;
		FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
		return Toolkit && Toolkit->GetPreview().GetMeshComponent() ? Toolkit : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersShownTest,
	"Cambriana.Editor.GrammarEditor.Parameters.DetailsShowTheTextsParametersGroupedByTierWithTheTextsValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersShownTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = ParameterGrammar();
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Grammar });

	const TMap<FName, TArray<FString>> Shown = ParameterRows(*Rows);
	TestEqual(TEXT("Four tier groups"), Shown.Num(), 4);
	TestTrue(TEXT("Pose Parameters holds bend"), Shown.FindRef(PoseCategory) == TArray<FString>{ TEXT("bend") });
	TestTrue(TEXT("Shape Parameters holds girth"), Shown.FindRef(ShapeCategory) == TArray<FString>{ TEXT("girth") });
	TestTrue(TEXT("Structure Parameters holds count"), Shown.FindRef(StructureCategory) == TArray<FString>{ TEXT("count") });
	TestTrue(TEXT("Parameters holds the untiered twice"), Shown.FindRef(UntieredCategory) == TArray<FString>{ TEXT("twice") });

	TestEqual(TEXT("bend shows the text's 10"), ShownValue(*Rows, TEXT("bend")), 10.0);
	TestEqual(TEXT("girth shows the text's 1.5"), ShownValue(*Rows, TEXT("girth")), 1.5);
	TestEqual(TEXT("count shows the text's 3"), ShownValue(*Rows, TEXT("count")), 3.0);
	TestEqual(TEXT("twice shows count * 2 = 6"), ShownValue(*Rows, TEXT("twice")), 6.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersOrderTest,
	"Cambriana.Editor.GrammarEditor.Parameters.ParametersOfOneTierAreShownInFileOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersOrderTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TEXT("structure param zeta = 1\nstructure param alpha = 2\nstructure param mid = 3\n\naxiom F\n");
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Grammar });
	TestTrue(TEXT("Structure Parameters lists zeta, alpha, mid"),
		ParameterRows(*Rows).FindRef(StructureCategory) == TArray<FString>{ TEXT("zeta"), TEXT("alpha"), TEXT("mid") });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersEditTest,
	"Cambriana.Editor.GrammarEditor.Parameters.EditingAParameterChangesTheWorkingCopyRebuildsThePreviewAndUndoRestoresIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersEditTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenWithText(Grammar, ParameterGrammar());
	if (!TestNotNull(TEXT("The grammar editor shows a preview"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Toolkit->GetWorkingCopy() });
	TestEqual(TEXT("The text's count of 3: 3 segments at 8 sides"), PreviewVertexCount(*Toolkit), TubeVertices(3, 8));
	const int32 BuildsBefore = Toolkit->GetPreview().GetBuildCount();

	Cambriana::Editor::SetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"), 5.0);
	TestEqual(TEXT("The edit rebuilt the preview once"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("count 5: 5 segments at 8 sides"), PreviewVertexCount(*Toolkit), TubeVertices(5, 8));
	const FCambrianaParameterValue* Stored = FindStoredValue(*Toolkit->GetWorkingCopy(), TEXT("count"));
	TestTrue(TEXT("The working copy stores count = 5"), Stored && Stored->Value == 5.0);
	TestEqual(TEXT("The grammar stores no parameter value"), Grammar->ParameterValues.Num(), 0);
	TestEqual(TEXT("Details shows count 5"), ShownValue(*Rows, TEXT("count")), 5.0);
	TestEqual(TEXT("Details shows twice following count: 10"), ShownValue(*Rows, TEXT("twice")), 10.0);

	Cambriana::Editor::SetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"), 5.0);
	TestEqual(TEXT("Setting the same value does not rebuild"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);

	TestTrue(TEXT("The edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo rebuilt the preview once"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 2);
	TestEqual(TEXT("Undo restores 3 segments"), PreviewVertexCount(*Toolkit), TubeVertices(3, 8));
	TestNull(TEXT("Undo removes the stored value"), FindStoredValue(*Toolkit->GetWorkingCopy(), TEXT("count")));
	TestEqual(TEXT("Details shows the text's 3 again"), ShownValue(*Rows, TEXT("count")), 3.0);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersResetTest,
	"Cambriana.Editor.GrammarEditor.Parameters.ResettingAParameterReturnsItToTheTextsValueUndoably",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersResetTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenWithText(Grammar, ParameterGrammar());
	if (!TestNotNull(TEXT("The grammar editor shows a preview"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	Cambriana::Editor::SetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"), 5.0);
	const int32 BuildsBefore = Toolkit->GetPreview().GetBuildCount();

	Cambriana::Editor::ResetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"));
	TestEqual(TEXT("The reset rebuilt the preview once"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);
	TestNull(TEXT("The working copy no longer stores count"), FindStoredValue(*Toolkit->GetWorkingCopy(), TEXT("count")));
	TestEqual(TEXT("The text's count of 3 applies"), PreviewVertexCount(*Toolkit), TubeVertices(3, 8));

	Cambriana::Editor::ResetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"));
	TestEqual(TEXT("Resetting an unchanged parameter does not rebuild"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);

	TestTrue(TEXT("The reset can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores count 5"), PreviewVertexCount(*Toolkit), TubeVertices(5, 8));
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersLegacyTest,
	"Cambriana.Editor.GrammarEditor.Parameters.LegacyTextShowsNoParameters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersLegacyTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->GrammarText = TEXT("3\n25\n50\nA\nA=F^'A\n@\n");
	FCambrianaParameterValue Stored;
	Stored.Name = TEXT("count");
	Stored.Value = 5.0;
	Grammar->ParameterValues.Add(Stored);
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Grammar });
	TestEqual(TEXT("No parameter group is shown"), ParameterRows(*Rows).Num(), 0);

	Grammar->GrammarText = ParameterGrammar();
	TestEqual(TEXT("The same asset with .lsys text shows its parameter groups"), ParameterRows(*MakeRows({ Grammar })).Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersTextValueTest,
	"Cambriana.Editor.GrammarEditor.Parameters.ATextValueChangeMovesUnchangedParametersAndLeavesStoredOnes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersTextValueTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenWithText(Grammar, ParameterGrammar(3));
	if (!TestNotNull(TEXT("The grammar editor shows a preview"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Toolkit->GetWorkingCopy() });

	Toolkit->CommitRulesText(FText::FromString(ParameterGrammar(4)));
	TestEqual(TEXT("Unchanged count follows the text to 4 segments"), PreviewVertexCount(*Toolkit), TubeVertices(4, 8));
	TestEqual(TEXT("Details shows the text's new 4"), ShownValue(*Rows, TEXT("count")), 4.0);

	Cambriana::Editor::SetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"), 5.0);
	Toolkit->CommitRulesText(FText::FromString(ParameterGrammar(2)));
	TestEqual(TEXT("Stored count 5 wins over the text's new 2"), PreviewVertexCount(*Toolkit), TubeVertices(5, 8));
	TestEqual(TEXT("Details still shows the stored 5"), ShownValue(*Rows, TEXT("count")), 5.0);
	const TArray<FCambrianaParameter> Read = Cambriana::ReadParameters(Toolkit->GetWorkingCopy()->GrammarText, Toolkit->GetWorkingCopy()->ParameterValues);
	TestTrue(TEXT("Resetting would return count to the text's new 2"), Read.IsValidIndex(2) && Read[2].TextValue == 2.0);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarParametersRenameTest,
	"Cambriana.Editor.GrammarEditor.Parameters.AddedRemovedAndRenamedParametersAreShownAsTheTextDeclaresThem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarParametersRenameTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	FCambrianaGrammarEditorToolkit* Toolkit = OpenWithText(Grammar, ParameterGrammar(3));
	if (!TestNotNull(TEXT("The grammar editor shows a preview"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Toolkit->GetWorkingCopy() });
	Cambriana::Editor::SetParameterValue(*Toolkit->GetWorkingCopy(), TEXT("count"), 5.0);

	Toolkit->CommitRulesText(FText::FromString(ParameterGrammar(3, TEXT("count"), TEXT("structure param extra = 2\n"))));
	TestTrue(TEXT("An added parameter is shown in its tier, in file order"),
		ParameterRows(*Rows).FindRef(StructureCategory) == TArray<FString>{ TEXT("count"), TEXT("extra") });
	TestEqual(TEXT("The added parameter shows the text's value"), ShownValue(*Rows, TEXT("extra")), 2.0);
	TestNull(TEXT("The added parameter has no stored value"), FindStoredValue(*Toolkit->GetWorkingCopy(), TEXT("extra")));

	Toolkit->CommitRulesText(FText::FromString(ParameterGrammar(3, TEXT("n"))));
	TestTrue(TEXT("The renamed parameter is shown under its new name"), ParameterRows(*Rows).FindRef(StructureCategory) == TArray<FString>{ TEXT("n") });
	TestEqual(TEXT("The renamed parameter uses the text's value"), ShownValue(*Rows, TEXT("n")), 3.0);
	TestEqual(TEXT("The text's 3 segments are built"), PreviewVertexCount(*Toolkit), TubeVertices(3, 8));
	const FCambrianaParameterValue* Kept = FindStoredValue(*Toolkit->GetWorkingCopy(), TEXT("count"));
	TestTrue(TEXT("The old name's stored value is kept"), Kept && Kept->Value == 5.0);

	Toolkit->CommitRulesText(FText::FromString(ParameterGrammar(3)));
	TestEqual(TEXT("The name declared again takes its stored 5"), PreviewVertexCount(*Toolkit), TubeVertices(5, 8));
	TestEqual(TEXT("Details shows the stored 5"), ShownValue(*Rows, TEXT("count")), 5.0);

	Toolkit->CommitRulesText(FText::FromString(TEXT("axiom F F\n")));
	TestEqual(TEXT("A text without parameters shows no parameter group"), ParameterRows(*Rows).Num(), 0);
	CloseGrammarEditor(Grammar);
	return true;
}

namespace
{
	TSharedPtr<SButton> NewSeedButtonOf(const IPropertyRowGenerator& Rows)
	{
		const TSharedPtr<IDetailTreeNode> Category = FindCategory(Rows, TEXT("Grammar"));
		TArray<TSharedRef<IDetailTreeNode>> Children;
		if (Category)
		{
			Category->GetChildren(Children);
		}
		for (const TSharedRef<IDetailTreeNode>& Child : Children)
		{
			const TSharedPtr<IPropertyHandle> Handle = Child->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() && Handle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed))
			{
				const TSharedPtr<SWidget> Value = Child->CreateNodeWidgets().ValueWidget;
				return Value ? StaticCastSharedPtr<SButton>(FindWidgetOfType(Value.ToSharedRef(), TEXT("SButton"))) : nullptr;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarTypedSeedTest,
	"Cambriana.Editor.GrammarEditor.Seed.ATypedSeedChangesTheWorkingCopysSeedAndRebuildsThePreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarTypedSeedTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->Seed = 1111;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Toolkit->GetWorkingCopy() });
	const TSharedPtr<IPropertyHandle> Seed = FindPropertyHandle(Rows->GetRootTreeNodes(), GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed));
	if (!TestTrue(TEXT("Details shows the seed"), Seed.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const int32 BuildsBefore = Toolkit->GetPreview().GetBuildCount();

	TestEqual(TEXT("The typed seed is accepted"), Seed->SetValue(uint32(2222)), FPropertyAccess::Success);
	TestEqual(TEXT("The working copy stores the typed seed"), Toolkit->GetWorkingCopy()->Seed, 2222u);
	TestEqual(TEXT("The grammar keeps its seed"), Grammar->Seed, 1111u);
	TestEqual(TEXT("The preview rebuilt once"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("The preview builds with the typed seed"), Toolkit->GetPreview().GetLastBuildInput().Seed, 2222u);
	CloseGrammarEditor(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarNewSeedButtonTest,
	"Cambriana.Editor.GrammarEditor.Seed.TheNewSeedButtonChangesTheWorkingCopysSeedAndRebuildsThePreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarNewSeedButtonTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	Grammar->Seed = 1111;
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const TSharedRef<IPropertyRowGenerator> Rows = MakeRows({ Toolkit->GetWorkingCopy() });
	const TSharedPtr<SButton> NewSeed = NewSeedButtonOf(*Rows);
	if (!TestTrue(TEXT("The seed row has a new-seed button"), NewSeed.IsValid()))
	{
		CloseGrammarEditor(Grammar);
		return false;
	}
	const int32 BuildsBefore = Toolkit->GetPreview().GetBuildCount();

	NewSeed->SimulateClick();
	TestNotEqual(TEXT("The working copy stores a new seed"), Toolkit->GetWorkingCopy()->Seed, 1111u);
	TestEqual(TEXT("The grammar keeps its seed"), Grammar->Seed, 1111u);
	TestEqual(TEXT("The preview rebuilt once"), Toolkit->GetPreview().GetBuildCount(), BuildsBefore + 1);
	TestEqual(TEXT("The preview builds with the new seed"), Toolkit->GetPreview().GetLastBuildInput().Seed, Toolkit->GetWorkingCopy()->Seed);
	CloseGrammarEditor(Grammar);
	return true;
}

#endif
