#include "CambrianaModule.h"

#include "CambrianaCoreLoader.h"
#include "Modules/ModuleManager.h"

void FCambrianaModule::StartupModule()
{
	CoreHandle = Cambriana::LoadCore();
}

void FCambrianaModule::ShutdownModule()
{
	if (CoreHandle)
	{
		FPlatformProcess::FreeDllHandle(CoreHandle);
		CoreHandle = nullptr;
	}
}

bool FCambrianaModule::IsCoreAvailable()
{
	const FCambrianaModule* Module = FModuleManager::GetModulePtr<FCambrianaModule>(TEXT("Cambriana"));
	return Module && Module->CoreHandle;
}

IMPLEMENT_MODULE(FCambrianaModule, Cambriana)
