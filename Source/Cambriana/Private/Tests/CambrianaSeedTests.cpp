#include "CambrianaSeed.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGeneratedSeedsDifferTest,
	"Cambriana.Seed.GeneratedSeedsDiffer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGeneratedSeedsDifferTest::RunTest(const FString& Parameters)
{
	TSet<uint32> Seeds;
	constexpr int32 Count = 8;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Seeds.Add(Cambriana::GenerateSeed());
	}
	TestEqual(TEXT("Every generated seed is distinct"), Seeds.Num(), Count);
	return true;
}

#endif
