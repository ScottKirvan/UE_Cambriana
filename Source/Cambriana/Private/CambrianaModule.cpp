#include "CambrianaModule.h"

#include "CambrianaCoreLoader.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "Modules/ModuleManager.h"

void FCambrianaModule::StartupModule()
{
	CoreHandle = Cambriana::LoadCore();
	GrammarChangedHandle = UCambrianaGrammar::OnGrammarChanged().AddStatic(&ACambrianaLiveActor::RebuildActorsUsing);
}

void FCambrianaModule::ShutdownModule()
{
	UCambrianaGrammar::OnGrammarChanged().Remove(GrammarChangedHandle);
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
