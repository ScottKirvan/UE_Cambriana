#pragma once

#include "CambrianaBuild.h"

#include <atomic>

struct cambriana_diagnostic;

namespace Cambriana
{
	FCambrianaDiagnostic ToDiagnostic(const cambriana_diagnostic& Core);

	/** Returns whether a build may call the core; the first refusal recorded in UnavailableLogged is logged. */
	bool ShouldBuildWithCore(bool bCoreAvailable, std::atomic<bool>& UnavailableLogged);

	/** Sets the report's flags from the core summary's CAMBRIANA_RESULT_* flags. */
	void ApplyResultFlags(uint32 Flags, FCambrianaBuildReport& Report);

	/** Logs the report's diagnostics, budgets and cancellation to LogCambriana. */
	void LogBuildReport(const FCambrianaBuildReport& Report);
}
