#include "CambrianaGrammar.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarImportDataLoadTest,
	"Cambriana.Grammar.ImportData.AGrammarBeingLoadedTakesItsSavedImportDataAndOneLoadedWithoutAnyGetsOne",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarImportDataLoadTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("A new grammar has import data"), NewObject<UCambrianaGrammar>(GetTransientPackage())->AssetImportData.Get());

	// As UTexture: an object constructed for loading leaves its import data to serialization, and PostLoad fills a gap.
	UCambrianaGrammar* Loading = NewObject<UCambrianaGrammar>(GetTransientPackage(), NAME_None, RF_NeedLoad | RF_NeedPostLoad);
	TestNull(TEXT("A grammar being loaded does not create import data its package will load"), Loading->AssetImportData.Get());
	Loading->ClearFlags(RF_NeedLoad);
	Loading->ConditionalPostLoad();
	TestNotNull(TEXT("A grammar loaded without import data gets some"), Loading->AssetImportData.Get());
	Loading->MarkAsGarbage();
	return true;
}

#endif
