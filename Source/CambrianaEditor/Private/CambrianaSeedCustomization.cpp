#include "CambrianaSeedCustomization.h"

#include "CambrianaSeed.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "CambrianaEditor"

namespace Cambriana::Editor
{
	void AssignNewSeeds(IPropertyHandle& SeedHandle)
	{
		TArray<FString> Values;
		for (int32 Index = 0; Index < SeedHandle.GetNumPerObjectValues(); ++Index)
		{
			Values.Add(LexToString(GenerateSeed()));
		}
		SeedHandle.SetPerObjectValues(Values);
	}
}

void Cambriana::Editor::CustomizeSeedRow(IDetailLayoutBuilder& DetailBuilder, FName SeedPropertyName, UClass* OwnerClass)
{
	const TSharedRef<IPropertyHandle> SeedHandle = DetailBuilder.GetProperty(SeedPropertyName, OwnerClass);
	IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(SeedHandle);
	if (!Row)
	{
		return;
	}

	Row->CustomWidget()
		.NameContent()
		[
			SeedHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SeedHandle->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewSeed", "New Seed"))
				.ToolTipText(LOCTEXT("NewSeedTooltip", "Generate a new random seed"))
				.OnClicked_Lambda([SeedHandle]
				{
					Cambriana::Editor::AssignNewSeeds(*SeedHandle);
					return FReply::Handled();
				})
			]
		];
}

#undef LOCTEXT_NAMESPACE
