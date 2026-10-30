#pragma once

#include "Modules/ModuleInterface.h"

class FCambrianaModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** True when the core library loaded and its API version matches. */
	static CAMBRIANA_API bool IsCoreAvailable();

private:
	void* CoreHandle = nullptr;
};
