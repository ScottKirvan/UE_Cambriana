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
	"Cambriana.LiveActor.Details.HeaderOverridesAreInlineEditConditionsAndSizeAndSurfacerOverridesAreLeftToTheCustomization",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorInlineOverrideTogglesTest::RunTest(const FString& Parameters)
{
	// Recursion, basic angle and thickness apply only with their override on, so an edit condition greys them exactly when they do
	// not apply. Size and surfacer values can apply with their override off; the Details customization shows those overrides inline
	// and greys each value by the build rule (Cambriana.Editor.LiveActorDetails).
	const TArray<FOverrideToggle> Toggles = {
		{ TEXT("bOverrideRecursion"), TEXT("Recursion"), { TEXT("Recursion") } },
		{ TEXT("bOverrideBasicAngle"), TEXT("BasicAngle"), { TEXT("BasicAngle") } },
		{ TEXT("bOverrideThickness"), TEXT("Thickness"), { TEXT("Thickness") } },
		{ TEXT("bOverrideMinThickness"), NAME_None, {} },
		{ TEXT("bOverrideClosedForm"), NAME_None, {} },
		{ TEXT("bOverrideSides"), NAME_None, {} },
		{ TEXT("bOverrideSize"), NAME_None, {} },
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
		TestEqual(FString::Printf(TEXT("%s is an inline edit condition toggle with no row of its own"), *Toggle), ToggleProperty->HasMetaData(InlineEditConditionToggle),
			!Expected.ShownOn.IsNone());

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
		TestEqual(FString::Printf(TEXT("%s's edit condition checkbox is shown on %s"), *Toggle, *Expected.ShownOn.ToString()), Join(ShownOn),
			Expected.ShownOn.IsNone() ? FString() : Expected.ShownOn.ToString());
	}
	return true;
}

#endif
