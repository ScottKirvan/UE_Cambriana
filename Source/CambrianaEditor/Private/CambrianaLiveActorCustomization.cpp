#include "CambrianaLiveActorCustomization.h"

#include "CambrianaBake.h"
#include "DetailCategoryBuilder.h"
#include "CambrianaLiveActor.h"
#include "CambrianaSeedCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CambrianaEditor"

namespace Cambriana::Editor
{
	namespace
	{
		template <typename FVisit>
		void ForEachLiveActor(const IPropertyHandle& Handle, FVisit&& Visit)
		{
			TArray<UObject*> Objects;
			Handle.GetOuterObjects(Objects);
			for (UObject* Object : Objects)
			{
				if (ACambrianaLiveActor* Actor = Cast<ACambrianaLiveActor>(Object))
				{
					Visit(*Actor);
				}
			}
		}

		FName PropertyNameOf(const IPropertyHandle& Handle)
		{
			return Handle.GetProperty() ? Handle.GetProperty()->GetFName() : NAME_None;
		}

		/** A setting whose value can apply with its override off, so its override is drawn on its row rather than as an edit condition. */
		struct FInlineOverride
		{
			FName Value;
			FName Override;
			bool bShowsCheckBox;
		};

		const FInlineOverride InlineOverrides[] = {
			{ GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, MinThickness), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideMinThickness), true },
			{ GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideClosedForm), true },
			{ GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideSides), true },
			{ GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideSize), true },
			{ GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bOverrideSize), false },
		};

		TSharedRef<SWidget> MakeOverrideCheckBox(const TSharedRef<IPropertyHandle>& Override)
		{
			return SNew(SCheckBox)
				.ToolTipText(Override->GetToolTipText())
				.IsChecked_Lambda([Override]
				{
					bool bOn = false;
					switch (Override->GetValue(bOn))
					{
					case FPropertyAccess::MultipleValues: return ECheckBoxState::Undetermined;
					case FPropertyAccess::Success: return bOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					default: return ECheckBoxState::Unchecked;
					}
				})
				.OnCheckStateChanged_Lambda([Override](ECheckBoxState State) { Override->SetValue(State == ECheckBoxState::Checked); });
		}

		void ShowOverrideInline(IDetailLayoutBuilder& DetailBuilder, IDetailPropertyRow& Row, const TSharedRef<IPropertyHandle>& Value, const FInlineOverride& Setting)
		{
			const TSharedRef<IPropertyHandle> Override = DetailBuilder.GetProperty(Setting.Override, ACambrianaLiveActor::StaticClass());
			DetailBuilder.HideProperty(Override);

			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			FDetailWidgetRow Defaults;
			Row.GetDefaultWidgets(NameWidget, ValueWidget, Defaults);
			const TAttribute<bool> Applies = TAttribute<bool>::CreateLambda([Value] { return UsesOwnValue(*Value); });

			Row.CustomWidget()
				.NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, Setting.bShowsCheckBox ? 4.0f : 0.0f, 0.0f)
					[
						Setting.bShowsCheckBox ? MakeOverrideCheckBox(Override) : SNullWidget::NullWidget
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(SBox).IsEnabled(Applies)[NameWidget.ToSharedRef()]
					]
				]
				.ValueContent()
				.MinDesiredWidth(Defaults.ValueWidget.MinWidth)
				.MaxDesiredWidth(Defaults.ValueWidget.MaxWidth)
				[
					SNew(SBox).IsEnabled(Applies)[ValueWidget.ToSharedRef()]
				];
		}
	}

	/** The placed live actors being customized; class defaults and archetypes have nothing to bake. */
	TArray<ACambrianaLiveActor*> PlacedLiveActors(const TArray<TWeakObjectPtr<UObject>>& Objects)
	{
		TArray<ACambrianaLiveActor*> Actors;
		for (const TWeakObjectPtr<UObject>& Object : Objects)
		{
			ACambrianaLiveActor* Actor = Cast<ACambrianaLiveActor>(Object.Get());
			if (Actor && !Actor->IsTemplate())
			{
				Actors.Add(Actor);
			}
		}
		return Actors;
	}

	void AddBakeRow(IDetailLayoutBuilder& DetailBuilder)
	{
		const FText BakeText = LOCTEXT("LiveActorBake", "Bake");
		const TArray<TWeakObjectPtr<UObject>> Objects = DetailBuilder.GetSelectedObjects();
		DetailBuilder.EditCategory(TEXT("Cambriana")).AddCustomRow(BakeText, false)
			.NameContent()
			[
				SNullWidget::NullWidget
			]
			.ValueContent()
			.VAlign(VAlign_Center)
			.MaxDesiredWidth(250.0f)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("LiveActorBakeTooltip", "Writes a static mesh from this actor's own settings, overrides included. Does not modify the actor."))
				.IsEnabled_Lambda([Objects] { return !PlacedLiveActors(Objects).IsEmpty(); })
				.OnClicked_Lambda([Objects]
				{
					for (ACambrianaLiveActor* Actor : PlacedLiveActors(Objects))
					{
						NotifyBakeResult(Actor->GetActorLabel(), BakeLiveActor(*Actor));
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(BakeText)
				]
			];
	}

	bool UsesOwnValue(const IPropertyHandle& SettingHandle)
	{
		const FName Name = PropertyNameOf(SettingHandle);
		bool bUses = false;
		ForEachLiveActor(SettingHandle, [&](const ACambrianaLiveActor& Actor) { bUses |= Actor.UsesOwnValue(Name); });
		return bUses;
	}

	bool DiffersFromStartingValue(const IPropertyHandle& SettingHandle)
	{
		const FName Name = PropertyNameOf(SettingHandle);
		bool bDiffers = false;
		ForEachLiveActor(SettingHandle, [&](const ACambrianaLiveActor& Actor) { bDiffers |= !Actor.IsAtStartingValue(Name); });
		return bDiffers;
	}

	void ResetToStartingValues(IPropertyHandle& SettingHandle)
	{
		const FName Name = PropertyNameOf(SettingHandle);
		const FScopedTransaction Transaction(LOCTEXT("ResetToStartingValue", "Reset to Default"));
		SettingHandle.NotifyPreChange();
		ForEachLiveActor(SettingHandle, [&](ACambrianaLiveActor& Actor) { Actor.ResetToStartingValue(Name); });
		SettingHandle.NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

TSharedRef<IDetailCustomization> FCambrianaLiveActorCustomization::MakeInstance()
{
	return MakeShared<FCambrianaLiveActorCustomization>();
}

void FCambrianaLiveActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Cambriana::Editor::CustomizeSeedRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Seed), ACambrianaLiveActor::StaticClass());
	for (const FName Name : ACambrianaLiveActor::GetStartingValueProperties())
	{
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(Name, ACambrianaLiveActor::StaticClass());
		IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(Handle);
		if (!Row)
		{
			continue;
		}
		Row->OverrideResetToDefault(FResetToDefaultOverride::Create(
			TAttribute<bool>::CreateLambda([Handle] { return Cambriana::Editor::DiffersFromStartingValue(*Handle); }),
			FSimpleDelegate::CreateLambda([Handle] { Cambriana::Editor::ResetToStartingValues(*Handle); })));
		for (const Cambriana::Editor::FInlineOverride& Setting : Cambriana::Editor::InlineOverrides)
		{
			if (Setting.Value == Name)
			{
				Cambriana::Editor::ShowOverrideInline(DetailBuilder, *Row, Handle, Setting);
			}
		}
	}
	Cambriana::Editor::AddBakeRow(DetailBuilder);
}

#undef LOCTEXT_NAMESPACE
