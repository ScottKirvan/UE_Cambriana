#include "CambrianaSeed.h"

#include <random>

namespace Cambriana
{
	uint32 GenerateSeed()
	{
		std::random_device Device;
		return static_cast<uint32>(Device());
	}
}
