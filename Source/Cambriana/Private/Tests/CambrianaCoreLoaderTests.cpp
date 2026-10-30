#include "CambrianaCoreLoader.h"
#include "CambrianaModule.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaApiVersionAcceptsMatchTest,
	"Cambriana.Core.ApiVersionCheck.AcceptsMatchingVersion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaApiVersionAcceptsMatchTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Matching versions report no error"), Cambriana::CheckCoreApiVersion(7, 7).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaApiVersionRejectsMismatchTest,
	"Cambriana.Core.ApiVersionCheck.RejectsMismatchedVersion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaApiVersionRejectsMismatchTest::RunTest(const FString& Parameters)
{
	for (const TPair<uint32, uint32>& Versions : { TPair<uint32, uint32>(2, 1), TPair<uint32, uint32>(1, 2) })
	{
		const TOptional<FString> Error = Cambriana::CheckCoreApiVersion(Versions.Key, Versions.Value);
		if (TestTrue(TEXT("Mismatched versions report an error"), Error.IsSet()))
		{
			TestTrue(TEXT("Error names the library version"), Error->Contains(FString::Printf(TEXT("%u"), Versions.Key)));
			TestTrue(TEXT("Error names the expected version"), Error->Contains(FString::Printf(TEXT("%u"), Versions.Value)));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaCoreLoadedAtStartupTest,
	"Cambriana.Core.LoadedAtStartup",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaCoreLoadedAtStartupTest::RunTest(const FString& Parameters)
{
	if (TestTrue(TEXT("Core is available after module startup"), FCambrianaModule::IsCoreAvailable()))
	{
		TestEqual(TEXT("Loaded core reports the header's API version"), cambriana_api_version(), CAMBRIANA_API_VERSION);
	}
	return true;
}

#endif
