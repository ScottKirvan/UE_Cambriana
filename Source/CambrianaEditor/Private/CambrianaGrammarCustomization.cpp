#include "CambrianaGrammarCustomization.h"

#include "CambrianaGrammar.h"
#include "CambrianaSeedCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CambrianaGrammarCustomization"

namespace
{
	constexpr ECambrianaParameterTier TierOrder[] = {
		ECambrianaParameterTier::Pose,
		ECambrianaParameterTier::Shape,
		ECambrianaParameterTier::Structure,
		ECambrianaParameterTier::None,
	};

	FName CategoryName(ECambrianaParameterTier Tier)
	{
		switch (Tier)
		{
		case ECambrianaParameterTier::Pose: return TEXT("Pose Parameters");
		case ECambrianaParameterTier::Shape: return TEXT("Shape Parameters");
		case ECambrianaParameterTier::Structure: return TEXT("Structure Parameters");
		default: return TEXT("Parameters");
		}
	}

	FText CategoryText(ECambrianaParameterTier Tier)
	{
		switch (Tier)
		{
		case ECambrianaParameterTier::Pose: return LOCTEXT("PoseParameters", "Pose Parameters");
		case ECambrianaParameterTier::Shape: return LOCTEXT("ShapeParameters", "Shape Parameters");
		case ECambrianaParameterTier::Structure: return LOCTEXT("StructureParameters", "Structure Parameters");
		default: return LOCTEXT("Parameters", "Parameters");
		}
	}

	bool SameRows(const TArray<FCambrianaParameter>& A, const TArray<FCambrianaParameter>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Tier != B[Index].Tier || !A[Index].Name.Equals(B[Index].Name, ESearchCase::CaseSensitive))
			{
				return false;
			}
		}
		return true;
	}

	int32 FindStoredIndex(const UCambrianaGrammar& Grammar, const FString& Name)
	{
		return Grammar.ParameterValues.IndexOfByPredicate([&Name](const FCambrianaParameterValue& Stored)
		{
			return Stored.Name.Equals(Name, ESearchCase::CaseSensitive);
		});
	}

	void EditParameterValues(UCambrianaGrammar& Grammar, const FText& Description, TFunctionRef<void(TArray<FCambrianaParameterValue>&)> Edit)
	{
		const FScopedTransaction Transaction(Description);
		FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, ParameterValues));
		Grammar.PreEditChange(Property);
		Edit(Grammar.ParameterValues);
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Grammar.PostEditChangeProperty(Event);
	}
}

namespace Cambriana::Editor
{
	void SetParameterValue(UCambrianaGrammar& Grammar, const FString& Name, double Value)
	{
		const int32 Index = FindStoredIndex(Grammar, Name);
		if (Index != INDEX_NONE && Grammar.ParameterValues[Index].Value == Value)
		{
			return;
		}
		EditParameterValues(Grammar, LOCTEXT("SetParameter", "Edit Grammar Parameter"), [&](TArray<FCambrianaParameterValue>& Values)
		{
			if (Index == INDEX_NONE)
			{
				FCambrianaParameterValue& Stored = Values.AddDefaulted_GetRef();
				Stored.Name = Name;
				Stored.Value = Value;
			}
			else
			{
				Values[Index].Value = Value;
			}
		});
	}

	void ResetParameterValue(UCambrianaGrammar& Grammar, const FString& Name)
	{
		const int32 Index = FindStoredIndex(Grammar, Name);
		if (Index == INDEX_NONE)
		{
			return;
		}
		EditParameterValues(Grammar, LOCTEXT("ResetParameter", "Reset Grammar Parameter"), [Index](TArray<FCambrianaParameterValue>& Values)
		{
			Values.RemoveAt(Index);
		});
	}
}

TSharedRef<IDetailCustomization> FCambrianaGrammarCustomization::MakeInstance()
{
	return MakeShareable(new FCambrianaGrammarCustomization());
}

FCambrianaGrammarCustomization::~FCambrianaGrammarCustomization()
{
	UCambrianaGrammar::OnGrammarChanged().Remove(GrammarChangedHandle);
}

void FCambrianaGrammarCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	LayoutBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

void FCambrianaGrammarCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Cambriana::Editor::CustomizeSeedRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed), UCambrianaGrammar::StaticClass());
	AddParameterRows(DetailBuilder);
}

void FCambrianaGrammarCustomization::AddParameterRows(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	Grammar = Objects.Num() == 1 ? Cast<UCambrianaGrammar>(Objects[0].Get()) : nullptr;
	if (!Grammar.IsValid())
	{
		return;
	}
	Parameters = Cambriana::ReadParameters(Grammar->GrammarText, Grammar->ParameterValues);
	GrammarChangedHandle = UCambrianaGrammar::OnGrammarChanged().AddRaw(this, &FCambrianaGrammarCustomization::HandleGrammarChanged);

	for (const ECambrianaParameterTier Tier : TierOrder)
	{
		IDetailCategoryBuilder* Category = nullptr;
		for (const FCambrianaParameter& Parameter : Parameters)
		{
			if (Parameter.Tier != Tier)
			{
				continue;
			}
			if (!Category)
			{
				Category = &DetailBuilder.EditCategory(CategoryName(Tier), CategoryText(Tier));
			}
			const FString Name = Parameter.Name;
			const FText NameText = FText::FromString(Name);
			Category->AddCustomRow(NameText)
				.RowTag(FName(*Name))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(NameText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				[
					SNew(SNumericEntryBox<double>)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Value_Lambda([this, Name]
					{
						const FCambrianaParameter* Shown = FindParameter(Name);
						return Shown ? TOptional<double>(Shown->Value) : TOptional<double>();
					})
					.OnValueCommitted_Lambda([this, Name](double NewValue, ETextCommit::Type)
					{
						if (UCambrianaGrammar* Edited = Grammar.Get())
						{
							Cambriana::Editor::SetParameterValue(*Edited, Name, NewValue);
						}
					})
				]
				.OverrideResetToDefault(FResetToDefaultOverride::Create(
					TAttribute<bool>::CreateLambda([this, Name]
					{
						const FCambrianaParameter* Shown = FindParameter(Name);
						return Shown && Shown->bStored;
					}),
					FSimpleDelegate::CreateLambda([this, Name]
					{
						if (UCambrianaGrammar* Edited = Grammar.Get())
						{
							Cambriana::Editor::ResetParameterValue(*Edited, Name);
						}
					})));
		}
	}
}

void FCambrianaGrammarCustomization::HandleGrammarChanged(const UCambrianaGrammar* ChangedGrammar)
{
	if (ChangedGrammar != Grammar.Get())
	{
		return;
	}
	TArray<FCambrianaParameter> Read = Cambriana::ReadParameters(ChangedGrammar->GrammarText, ChangedGrammar->ParameterValues);
	const bool bRowsChanged = !SameRows(Read, Parameters);
	Parameters = MoveTemp(Read);
	// Refreshing replaces this customization, so nothing may use it afterwards. The refresh
	// checks that the details view holds the only reference to the layout, so none is kept here.
	if (IDetailLayoutBuilder* Builder = bRowsChanged ? LayoutBuilder.Pin().Get() : nullptr)
	{
		Builder->ForceRefreshDetails();
	}
}

const FCambrianaParameter* FCambrianaGrammarCustomization::FindParameter(const FString& Name) const
{
	return Parameters.FindByPredicate([&Name](const FCambrianaParameter& Parameter)
	{
		return Parameter.Name.Equals(Name, ESearchCase::CaseSensitive);
	});
}

#undef LOCTEXT_NAMESPACE
