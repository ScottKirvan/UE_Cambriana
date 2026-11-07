#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSettingsDefaultsTest,
	"Cambriana.GrammarAsset.Settings.DefaultsMatchTheLiveActorDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
	const ACambrianaLiveActor* Actor = GetDefault<ACambrianaLiveActor>();
	TestTrue(TEXT("Size mode"), Grammar->SizeMode == Actor->SizeMode);
	TestEqual(TEXT("Size value"), Grammar->SizeValue, Actor->SizeValue);
	TestEqual(TEXT("Closed form"), Grammar->bClosedForm, Actor->bClosedForm);
	TestEqual(TEXT("Sides"), Grammar->Sides, Actor->Sides);
	TestEqual(TEXT("Min thickness"), Grammar->MinThickness, Actor->MinThickness);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSettingsEditableTest,
	"Cambriana.GrammarAsset.Settings.AreEditableOnTheAsset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSettingsEditableTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("SizeMode"), TEXT("SizeValue"), TEXT("bClosedForm"), TEXT("Sides"), TEXT("MinThickness") })
	{
		const FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(Name);
		if (TestNotNull(FString::Printf(TEXT("%s is a property"), Name), Property))
		{
			TestTrue(FString::Printf(TEXT("%s is editable"), Name), Property->HasAnyPropertyFlags(CPF_Edit));
			TestFalse(FString::Printf(TEXT("%s is not read-only"), Name), Property->HasAnyPropertyFlags(CPF_EditConst));
		}
	}
	return true;
}

#endif
