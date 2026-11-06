#include "CambrianaGrammar.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarFormat.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

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

	class FCoreResult
	{
	public:
		FCoreResult(ECambrianaGrammarFormat Format, const FString& Text, uint32 Seed)
		{
			cambriana_build_settings Settings;
			cambriana_build_settings_init(&Settings, sizeof(Settings));
			Settings.seed = Seed;
			Settings.overrides |= CAMBRIANA_OVERRIDE_SEED;
			const FTCHARToUTF8 Utf8(*Text);
			if (Format == ECambrianaGrammarFormat::Legacy)
			{
				cambriana_build_legacy(Utf8.Get(), Utf8.Length(), &Settings, nullptr, &Result);
			}
			else
			{
				cambriana_build_grammar(Utf8.Get(), Utf8.Length(), nullptr, &Settings, nullptr, &Result);
			}
		}

		~FCoreResult() { cambriana_result_destroy(Result); }

		FCoreResult(const FCoreResult&) = delete;
		FCoreResult& operator=(const FCoreResult&) = delete;

		bool IsValid() const { return Result != nullptr; }

		bool HasErrors() const { return (cambriana_result_summary(Result)->flags & CAMBRIANA_RESULT_HAS_ERRORS) != 0; }

		TConstArrayView<cambriana_bone> Bones() const
		{
			uint64_t Count = 0;
			const cambriana_bone* Data = cambriana_result_bones(Result, &Count);
			return TConstArrayView<cambriana_bone>(Data, static_cast<int32>(Count));
		}

	private:
		cambriana_result* Result = nullptr;
	};

	bool operator==(const cambriana_vec3& A, const cambriana_vec3& B)
	{
		return A.x == B.x && A.y == B.y && A.z == B.z;
	}

	bool SameBone(const cambriana_bone& A, const cambriana_bone& B)
	{
		return A.start == B.start && A.end == B.end && A.frame.side == B.frame.side && A.frame.forward == B.frame.forward
			&& A.frame.up == B.frame.up && A.radius == B.radius && A.parent == B.parent && A.colour == B.colour
			&& A.generation == B.generation && A.kind == B.kind && A.symbol == B.symbol && A.chain_break_before == B.chain_break_before
			&& FCStringAnsi::Strcmp(A.name ? A.name : "", B.name ? B.name : "") == 0;
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
	int32 Mismatches = 0;
	for (int32 Index = 0; Index < LegacyBones.Num(); ++Index)
	{
		Mismatches += !SameBone(NewBones[Index], LegacyBones[Index]);
	}
	TestEqual(TEXT("Bones that differ from the legacy default's"), Mismatches, 0);
	return true;
}

#endif
