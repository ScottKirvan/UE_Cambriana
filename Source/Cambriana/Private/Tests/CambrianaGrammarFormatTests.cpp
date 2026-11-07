#include "CambrianaGrammarFormat.h"
#include "CambrianaTestUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString FormatName(ECambrianaGrammarFormat Format)
	{
		return Format == ECambrianaGrammarFormat::Legacy ? TEXT("Legacy") : TEXT("Cambriana");
	}

	void ExpectFormat(FAutomationTestBase& Test, const FString& Label, const FString& Text, ECambrianaGrammarFormat Expected)
	{
		Test.TestEqual(Label, FormatName(Cambriana::DetectGrammarFormat(Text)), FormatName(Expected));
	}

	void ExpectFormats(FAutomationTestBase& Test, std::initializer_list<const TCHAR*> Texts, ECambrianaGrammarFormat Expected)
	{
		for (const TCHAR* Text : Texts)
		{
			ExpectFormat(Test, FString::Printf(TEXT("'%s'"), *FString(Text).ReplaceCharWithEscapedChar()), Text, Expected);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarFormatHeaderNumberTest,
	"Cambriana.GrammarFormat.HeaderNumberIsLegacy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarFormatHeaderNumberTest::RunTest(const FString& Parameters)
{
	ExpectFormats(*this, { TEXT("20\n25\n50\nA\nA=F\n@\n"), TEXT("2.5\n"), TEXT("-3 # recursion\n"), TEXT("+4\n"), TEXT(".5\n"), TEXT("  \t7\n"), TEXT("12") },
		ECambrianaGrammarFormat::Legacy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarFormatLeadingLinesTest,
	"Cambriana.GrammarFormat.LeadingCommentsAndBlankLinesAreSkipped",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarFormatLeadingLinesTest::RunTest(const FString& Parameters)
{
	ExpectFormats(*this, { TEXT("#\tsk.ls\n20\n"), TEXT("\n\n20\n"), TEXT("\r\n# a\r\n   \r\n  # b\r\n20\r\n"), TEXT("# a\r20\r") },
		ECambrianaGrammarFormat::Legacy);
	ExpectFormat(*this, TEXT("sk.ls fixture"), Cambriana::Tests::LoadFixture(TEXT("sk.ls")), ECambrianaGrammarFormat::Legacy);
	ExpectFormat(*this, TEXT("TREE11.LS fixture"), Cambriana::Tests::LoadFixture(TEXT("TREE11.LS")), ECambrianaGrammarFormat::Legacy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarFormatLsysTest,
	"Cambriana.GrammarFormat.TextWithoutHeaderNumberIsCambriana",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarFormatLsysTest::RunTest(const FString& Parameters)
{
	ExpectFormats(*this, { TEXT("generations 20\naxiom F\n"), TEXT("# 20 generations\n\n# 25\naxiom F\n"), TEXT("  # 1\n  axiom F # 2\n"), TEXT("axiom F\n20\n"), TEXT("-F\n") },
		ECambrianaGrammarFormat::Cambriana);
	const FString Sk = Cambriana::Tests::LoadFixture(TEXT("sk.lsys"));
	TestFalse(TEXT("sk.lsys fixture loaded"), Sk.IsEmpty());
	ExpectFormat(*this, TEXT("sk.lsys fixture"), Sk, ECambrianaGrammarFormat::Cambriana);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarFormatEmptyTest,
	"Cambriana.GrammarFormat.EmptyOrCommentOnlyTextIsCambriana",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarFormatEmptyTest::RunTest(const FString& Parameters)
{
	ExpectFormats(*this, { TEXT(""), TEXT("\n\n"), TEXT("   \t\n"), TEXT("# 20\n# 25\n") }, ECambrianaGrammarFormat::Cambriana);
	return true;
}

#endif
