#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPluginProducerTest,
	"Cambriana.Plugin.ProducerIsBojuStudiosAndDescriptionCreditsLparser",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPluginProducerTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Cambriana"));
	if (!TestTrue(TEXT("The Cambriana plugin is loaded"), Plugin.IsValid()))
	{
		return false;
	}
	const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
	TestEqual(TEXT("The plugin's producer"), Descriptor.CreatedBy, FString(TEXT("Boju Studios")));
	TestTrue(TEXT("The description keeps the Lparser credit"),
		Descriptor.Description.Contains(TEXT("Reads Lparser .ls files. Lparser by Laurens Lapré."), ESearchCase::CaseSensitive));
	return true;
}

#endif
