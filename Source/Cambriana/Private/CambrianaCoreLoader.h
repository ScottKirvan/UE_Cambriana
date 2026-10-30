#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCambriana, Log, All);

namespace Cambriana
{
	/** Returns an error message unless the loaded library's API version equals the version the plugin was built against. */
	TOptional<FString> CheckCoreApiVersion(uint32 LibraryVersion, uint32 ExpectedVersion);

	/** Loads the core dynamic library and checks its API version. Returns the handle, or null after logging an error. */
	void* LoadCore();
}
