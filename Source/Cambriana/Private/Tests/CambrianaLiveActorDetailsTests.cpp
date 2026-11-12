#include "CambrianaLiveActor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_METADATA

namespace
{
	struct FOverrideToggle
	{
		FName Toggle;
		FName ShownOn;
		TArray<FName> Controls;
	};

	const FName InlineEditConditionToggle(TEXT("InlineEditConditionToggle"));
	const FName EditCondition(TEXT("EditCondition"));
	const FName HideEditConditionToggle(TEXT("HideEditConditionToggle"));

	FString Join(TArray<FName> Names)
	{
		Names.Sort(FNameLexicalLess());
		return FString::JoinBy(Names, TEXT(", "), [](FName Name) { return Name.ToString(); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorInlineOverrideTogglesTest,
	"Cambriana.LiveActor.Details.EachOverrideCheckboxIsInlineLeftOfTheValuesItControls",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorInlineOverrideTogglesTest::RunTest(const FString& Parameters)
{
	const TArray<FOverrideToggle> Toggles = {
		{ TEXT("bOverrideRecursion"), TEXT("Recursion"), { TEXT("Recursion") } },
		{ TEXT("bOverrideBasicAngle"), TEXT("BasicAngle"), { TEXT("BasicAngle") } },
		{ TEXT("bOverrideThickness"), TEXT("Thickness"), { TEXT("Thickness") } },
		{ TEXT("bOverrideMinThickness"), TEXT("MinThickness"), { TEXT("MinThickness") } },
		{ TEXT("bOverrideClosedForm"), TEXT("bClosedForm"), { TEXT("bClosedForm") } },
		{ TEXT("bOverrideSides"), TEXT("Sides"), { TEXT("Sides") } },
		{ TEXT("bOverrideSize"), TEXT("SizeMode"), { TEXT("SizeMode"), TEXT("SizeValue") } },
	};

	const UClass* Class = ACambrianaLiveActor::StaticClass();
	for (const FOverrideToggle& Expected : Toggles)
	{
		const FString Toggle = Expected.Toggle.ToString();
		const FProperty* ToggleProperty = Class->FindPropertyByName(Expected.Toggle);
		if (!TestNotNull(FString::Printf(TEXT("%s is a property"), *Toggle), ToggleProperty))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s is a bool"), *Toggle), ToggleProperty->IsA<FBoolProperty>());
		TestTrue(FString::Printf(TEXT("%s is editable"), *Toggle), ToggleProperty->HasAnyPropertyFlags(CPF_Edit));
		TestTrue(FString::Printf(TEXT("%s has no row of its own, only an inline checkbox"), *Toggle), ToggleProperty->HasMetaData(InlineEditConditionToggle));

		TArray<FName> Controlled;
		TArray<FName> ShownOn;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (It->GetMetaData(EditCondition) == Toggle)
			{
				Controlled.Add(It->GetFName());
				if (!It->HasMetaData(HideEditConditionToggle))
				{
					ShownOn.Add(It->GetFName());
				}
			}
		}
		TestEqual(FString::Printf(TEXT("%s's checkbox enables exactly its values"), *Toggle), Join(Controlled), Join(Expected.Controls));
		TestEqual(FString::Printf(TEXT("%s's checkbox is shown once, to the left of %s"), *Toggle, *Expected.ShownOn.ToString()), Join(ShownOn), Expected.ShownOn.ToString());
	}
	return true;
}

#endif
