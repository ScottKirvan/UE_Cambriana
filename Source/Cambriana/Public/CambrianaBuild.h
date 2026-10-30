#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

#include "CambrianaBuild.generated.h"

UENUM()
enum class ECambrianaSizeMode : uint8
{
	Fit,
	Step,
};

/** Settings for one build. Unset header settings fall back to the grammar's own values. */
struct FCambrianaBuildSettings
{
	TOptional<double> Recursion;
	TOptional<double> BasicAngle;
	TOptional<double> Thickness;
	double MinThickness = 0.0;
	bool bClosedForm = true;
	int32 Sides = 8;
	ECambrianaSizeMode SizeMode = ECambrianaSizeMode::Fit;
	double SizeValue = 200.0;

	bool operator==(const FCambrianaBuildSettings&) const = default;
};

struct FCambrianaBuildInput
{
	FString GrammarText;
	FCambrianaBuildSettings Settings;
	uint32 Seed = 0;

	bool operator==(const FCambrianaBuildInput&) const = default;
};

struct FCambrianaBuildResult
{
	UE::Geometry::FDynamicMesh3 Mesh;
};

namespace Cambriana
{
	/** Generates a mesh in Unreal space (left-handed, +Z up, cm) from grammar text, settings and seed. */
	CAMBRIANA_API FCambrianaBuildResult BuildMesh(const FCambrianaBuildInput& Input);
}
