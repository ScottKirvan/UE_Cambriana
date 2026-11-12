#include "CambrianaLiveActorCustomization.h"

#include "CambrianaLiveActor.h"
#include "CambrianaSeedCustomization.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"

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
	}

	bool DiffersFromStartingValue(const IPropertyHandle& SettingHandle)
	{
		const FName Name = SettingHandle.GetProperty() ? SettingHandle.GetProperty()->GetFName() : NAME_None;
		bool bDiffers = false;
		ForEachLiveActor(SettingHandle, [&](const ACambrianaLiveActor& Actor) { bDiffers |= !Actor.IsAtStartingValue(Name); });
		return bDiffers;
	}

	void ResetToStartingValues(IPropertyHandle& SettingHandle)
	{
		const FName Name = SettingHandle.GetProperty() ? SettingHandle.GetProperty()->GetFName() : NAME_None;
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
		if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(Handle))
		{
			Row->OverrideResetToDefault(FResetToDefaultOverride::Create(
				TAttribute<bool>::CreateLambda([Handle] { return Cambriana::Editor::DiffersFromStartingValue(*Handle); }),
				FSimpleDelegate::CreateLambda([Handle] { Cambriana::Editor::ResetToStartingValues(*Handle); })));
		}
	}
}

#undef LOCTEXT_NAMESPACE
