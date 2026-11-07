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

/** Settings for one build. Unset header settings fall back to the grammar's own values; unset budgets to the core's defaults. */
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
	/** Cambriana grammar text uses SizeMode and SizeValue only when set; otherwise its own size or step, else the core's default. */
	bool bOverrideGrammarSize = false;
	/** Cambriana grammar text stating neither size nor step uses SizeMode and SizeValue instead of the core's default. */
	bool bSizeForGrammarWithoutSize = false;
	TOptional<uint64> ProductionBudget;
	TOptional<uint64> BoneBudget;
	TOptional<uint64> WorkBudget;
	TOptional<uint64> VertexBudget;

	bool operator==(const FCambrianaBuildSettings&) const = default;
};

struct FCambrianaBuildInput
{
	FString GrammarText;
	FCambrianaBuildSettings Settings;
	uint32 Seed = 0;

	bool operator==(const FCambrianaBuildInput&) const = default;
};

enum class ECambrianaDiagnosticSeverity : uint8
{
	Warning,
	Error,
};

enum class ECambrianaDiagnosticCode : uint8
{
	Unknown,
	MissingEndMarker,
	InvalidNumber,
	MissingSetting,
	MissingAxiom,
	MalformedRule,
	UnmatchedPop,
	UnmatchedPolygonEnd,
	UnclosedPolygon,
	Syntax,
	CommandUsedAsName,
	ReservedName,
	UnknownName,
	DuplicateName,
	ArgumentCount,
	PoseParameter,
	MacroCycle,
	ModuleInDrawRule,
	UnknownAxiom,
	SegmentReplacedBySeveral,
	UnreachableRule,
	NotConstant,
	SizeAndStep,
	NonFiniteValue,
	NotConvertible,
	ShapeParameter,
};

/** A grammar diagnostic. Parse diagnostics carry a 1-based line and column; turtle diagnostics a production position instead. */
struct FCambrianaDiagnostic
{
	ECambrianaDiagnosticSeverity Severity = ECambrianaDiagnosticSeverity::Warning;
	ECambrianaDiagnosticCode Code = ECambrianaDiagnosticCode::Unknown;
	uint64 Line = 0;
	uint64 Column = 0;
	TOptional<uint64> ProductionPosition;

	CAMBRIANA_API FString Describe() const;
};

/** What a build reported besides its mesh. */
struct FCambrianaBuildReport
{
	TArray<FCambrianaDiagnostic> Diagnostics;
	bool bCoreUnavailable = false;
	bool bHasErrors = false;
	bool bProductionBudgetReached = false;
	bool bBoneBudgetReached = false;
	bool bWorkBudgetReached = false;
	bool bVertexBudgetReached = false;
	bool bCancelled = false;
	uint64 ProductionLength = 0;
	uint64 GenerationsRun = 0;
	double Recursion = 0.0;
	double BasicAngle = 0.0;
	double Thickness = 0.0;
	double StepLength = 0.0;
	/** The size mode and value the build used; in Step mode the value is StepLength. */
	ECambrianaSizeMode SizeMode = ECambrianaSizeMode::Fit;
	double SizeValue = 0.0;
	/** The settings above were reported by a core build. */
	bool bHasSummary = false;
};

struct FCambrianaBuildResult
{
	/** Each triangle's material ID is a slot index. */
	UE::Geometry::FDynamicMesh3 Mesh;
	/** The core material ID of each slot, ascending. */
	TArray<int64> SlotMaterialIds;
	FCambrianaBuildReport Report;
};

namespace Cambriana
{
	/** Generates a mesh in Unreal space (left-handed, +Z up, cm) from grammar text, settings and seed. */
	CAMBRIANA_API FCambrianaBuildResult BuildMesh(const FCambrianaBuildInput& Input);

	constexpr int32 PreviewPaletteSize = 10;

	/** Index of a core material ID in the preview palette: IDs 0 to 9, repeating. */
	CAMBRIANA_API int32 PreviewPaletteIndex(int64 MaterialId);

	/** Preview colour of a core material ID: black, brown, red, orange, yellow, green, blue, violet, grey, white. */
	CAMBRIANA_API FLinearColor PreviewPaletteColor(int64 MaterialId);
}
