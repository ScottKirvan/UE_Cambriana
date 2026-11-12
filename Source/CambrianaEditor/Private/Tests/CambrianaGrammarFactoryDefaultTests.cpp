#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarFormat.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	const TCHAR* LegacyDefaultGrammarText = TEXT("4\n25\n50\nA\nA=F^'A\n@\n");

	FString NewAssetGrammarText()
	{
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		const UCambrianaGrammar* Grammar = Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(),
			GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass()), RF_Transactional,
			nullptr, nullptr));
		return Grammar ? Grammar->GrammarText : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaNewAssetHoldsLsysTest,
	"Cambriana.Editor.GrammarFactory.NewAssetHoldsLsysThatParsesWithoutErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaNewAssetHoldsLsysTest::RunTest(const FString& Parameters)
{
	const FString Text = NewAssetGrammarText();
	TestTrue(TEXT("Detection says .lsys"), Cambriana::DetectGrammarFormat(Text) == ECambrianaGrammarFormat::Cambriana);

	const FTCHARToUTF8 Utf8(*Text);
	cambriana_grammar* Grammar = nullptr;
	if (!TestEqual(TEXT("The core parses the text"), cambriana_grammar_parse(Utf8.Get(), Utf8.Length(), &Grammar),
			static_cast<cambriana_status>(CAMBRIANA_OK)))
	{
		return false;
	}
	uint64_t DiagnosticCount = 0;
	cambriana_grammar_diagnostics(Grammar, &DiagnosticCount);
	TestEqual(TEXT("The core reports no diagnostics"), DiagnosticCount, uint64_t(0));
	cambriana_grammar_destroy(Grammar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaNewAssetBuildsLegacyDefaultBonesTest,
	"Cambriana.Editor.GrammarFactory.NewAssetBuildsTheSameBonesAsTheLegacyDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaNewAssetBuildsLegacyDefaultBonesTest::RunTest(const FString& Parameters)
{
	const FCoreResult Legacy(ECambrianaGrammarFormat::Legacy, LegacyDefaultGrammarText, 12345);
	const FCoreResult New(ECambrianaGrammarFormat::Cambriana, NewAssetGrammarText(), 12345);
	if (!TestTrue(TEXT("Legacy default built"), Legacy.IsValid()) || !TestTrue(TEXT("New asset's grammar built"), New.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("Legacy default has no errors"), Legacy.HasErrors());
	TestFalse(TEXT("New asset's grammar has no errors"), New.HasErrors());

	const TConstArrayView<cambriana_bone> LegacyBones = Legacy.Bones();
	const TConstArrayView<cambriana_bone> NewBones = New.Bones();
	TestTrue(TEXT("Legacy default has bones"), LegacyBones.Num() > 0);
	if (!TestEqual(TEXT("Bone count"), NewBones.Num(), LegacyBones.Num()))
	{
		return false;
	}
	TestEqual(TEXT("Bones that differ from the legacy default's"), CountBoneMismatches(NewBones, LegacyBones), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaNewAssetSettingsTest,
	"Cambriana.Editor.GrammarFactory.NewAssetSizeAndSurfacerSettingsAreTheDefaultsTheDefaultTextLeaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaNewAssetSettingsTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
	const UCambrianaGrammar* Grammar = Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(),
		GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass()), RF_Transactional,
		nullptr, nullptr));
	if (!TestNotNull(TEXT("New asset created"), Grammar))
	{
		return false;
	}
	TestEqual(TEXT("The default text states no size or surfacer setting, so each is the default"), DescribeSizeAndSurfacer(*Grammar),
		FString(TEXT("size Fit 200, sides 8, surface tubes, min thickness 0")));
	return true;
}

#endif
