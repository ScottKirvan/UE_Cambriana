#include "CambrianaCoreLoader.h"

#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include <cambriana/cambriana.h>

DEFINE_LOG_CATEGORY(LogCambriana);

namespace Cambriana
{
	static const TCHAR* CoreLibraryName = TEXT("CambrianaCore.dll");

	TOptional<FString> CheckCoreApiVersion(uint32 LibraryVersion, uint32 ExpectedVersion)
	{
		if (LibraryVersion == ExpectedVersion)
		{
			return {};
		}
		return FString::Printf(TEXT("CambrianaCore API version %u does not match the version this plugin was built against (%u)"),
			LibraryVersion, ExpectedVersion);
	}

	static FString FindCoreLibrary()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Cambriana")))
		{
			const FString PluginBinary = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Binaries"),
				FPlatformProcess::GetBinariesSubdirectory(), CoreLibraryName);
			if (FPaths::FileExists(PluginBinary))
			{
				return PluginBinary;
			}
		}
		// Monolithic builds stage the library beside the executable, where the loader finds it by name.
		return CoreLibraryName;
	}

	void* LoadCore()
	{
		const FString LibraryPath = FindCoreLibrary();
		void* Handle = FPlatformProcess::GetDllHandle(*LibraryPath);
		if (!Handle)
		{
			UE_LOG(LogCambriana, Error, TEXT("Could not load %s; Cambriana is unavailable"), *LibraryPath);
			return nullptr;
		}

		// cambriana_api_version keeps its signature in every API version, so it is safe to call before the check.
		if (const TOptional<FString> Error = CheckCoreApiVersion(cambriana_api_version(), CAMBRIANA_API_VERSION))
		{
			UE_LOG(LogCambriana, Error, TEXT("%s (%s); Cambriana is unavailable"), **Error, *LibraryPath);
			FPlatformProcess::FreeDllHandle(Handle);
			return nullptr;
		}

		UE_LOG(LogCambriana, Log, TEXT("Loaded %s, API version %u"), *LibraryPath, CAMBRIANA_API_VERSION);
		return Handle;
	}
}
