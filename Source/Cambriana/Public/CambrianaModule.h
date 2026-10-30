#pragma once

#include "Modules/ModuleInterface.h"

class FCambrianaModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
