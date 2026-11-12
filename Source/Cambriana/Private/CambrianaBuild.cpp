#include "CambrianaBuild.h"

#include "Algo/BinarySearch.h"
#include "CambrianaBuildInternal.h"
#include "CambrianaCoreLoader.h"
#include "CambrianaGrammarFormat.h"
#include "CambrianaModule.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"

#include <cambriana/cambriana.h>

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FIndex3i;

namespace
{
	constexpr double NormalSplitAngleDegrees = 60.0;

	ECambrianaDiagnosticCode ToDiagnosticCode(int32 Code)
	{
		switch (Code)
		{
		case CAMBRIANA_DIAGNOSTIC_MISSING_END_MARKER: return ECambrianaDiagnosticCode::MissingEndMarker;
		case CAMBRIANA_DIAGNOSTIC_INVALID_NUMBER: return ECambrianaDiagnosticCode::InvalidNumber;
		case CAMBRIANA_DIAGNOSTIC_MISSING_SETTING: return ECambrianaDiagnosticCode::MissingSetting;
		case CAMBRIANA_DIAGNOSTIC_MISSING_AXIOM: return ECambrianaDiagnosticCode::MissingAxiom;
		case CAMBRIANA_DIAGNOSTIC_MALFORMED_RULE: return ECambrianaDiagnosticCode::MalformedRule;
		case CAMBRIANA_DIAGNOSTIC_UNMATCHED_POP: return ECambrianaDiagnosticCode::UnmatchedPop;
		case CAMBRIANA_DIAGNOSTIC_UNMATCHED_POLYGON_END: return ECambrianaDiagnosticCode::UnmatchedPolygonEnd;
		case CAMBRIANA_DIAGNOSTIC_UNCLOSED_POLYGON: return ECambrianaDiagnosticCode::UnclosedPolygon;
		case CAMBRIANA_DIAGNOSTIC_SYNTAX: return ECambrianaDiagnosticCode::Syntax;
		case CAMBRIANA_DIAGNOSTIC_COMMAND_USED_AS_NAME: return ECambrianaDiagnosticCode::CommandUsedAsName;
		case CAMBRIANA_DIAGNOSTIC_RESERVED_NAME: return ECambrianaDiagnosticCode::ReservedName;
		case CAMBRIANA_DIAGNOSTIC_UNKNOWN_NAME: return ECambrianaDiagnosticCode::UnknownName;
		case CAMBRIANA_DIAGNOSTIC_DUPLICATE_NAME: return ECambrianaDiagnosticCode::DuplicateName;
		case CAMBRIANA_DIAGNOSTIC_ARGUMENT_COUNT: return ECambrianaDiagnosticCode::ArgumentCount;
		case CAMBRIANA_DIAGNOSTIC_POSE_PARAMETER: return ECambrianaDiagnosticCode::PoseParameter;
		case CAMBRIANA_DIAGNOSTIC_MACRO_CYCLE: return ECambrianaDiagnosticCode::MacroCycle;
		case CAMBRIANA_DIAGNOSTIC_MODULE_IN_DRAW_RULE: return ECambrianaDiagnosticCode::ModuleInDrawRule;
		case CAMBRIANA_DIAGNOSTIC_UNKNOWN_AXIOM: return ECambrianaDiagnosticCode::UnknownAxiom;
		case CAMBRIANA_DIAGNOSTIC_SEGMENT_REPLACED_BY_SEVERAL: return ECambrianaDiagnosticCode::SegmentReplacedBySeveral;
		case CAMBRIANA_DIAGNOSTIC_UNREACHABLE_RULE: return ECambrianaDiagnosticCode::UnreachableRule;
		case CAMBRIANA_DIAGNOSTIC_NOT_CONSTANT: return ECambrianaDiagnosticCode::NotConstant;
		case CAMBRIANA_DIAGNOSTIC_SIZE_AND_STEP: return ECambrianaDiagnosticCode::SizeAndStep;
		case CAMBRIANA_DIAGNOSTIC_NON_FINITE_VALUE: return ECambrianaDiagnosticCode::NonFiniteValue;
		case CAMBRIANA_DIAGNOSTIC_NOT_CONVERTIBLE: return ECambrianaDiagnosticCode::NotConvertible;
		case CAMBRIANA_DIAGNOSTIC_SHAPE_PARAMETER: return ECambrianaDiagnosticCode::ShapeParameter;
		default: return ECambrianaDiagnosticCode::Unknown;
		}
	}

	const TCHAR* DescribeCode(ECambrianaDiagnosticCode Code)
	{
		switch (Code)
		{
		case ECambrianaDiagnosticCode::MissingEndMarker: return TEXT("missing end marker '@'");
		case ECambrianaDiagnosticCode::InvalidNumber: return TEXT("invalid number");
		case ECambrianaDiagnosticCode::MissingSetting: return TEXT("missing setting");
		case ECambrianaDiagnosticCode::MissingAxiom: return TEXT("missing axiom");
		case ECambrianaDiagnosticCode::MalformedRule: return TEXT("malformed rule");
		case ECambrianaDiagnosticCode::UnmatchedPop: return TEXT("unmatched ']'");
		case ECambrianaDiagnosticCode::UnmatchedPolygonEnd: return TEXT("unmatched '}'");
		case ECambrianaDiagnosticCode::UnclosedPolygon: return TEXT("unclosed polygon");
		case ECambrianaDiagnosticCode::Syntax: return TEXT("syntax error");
		case ECambrianaDiagnosticCode::CommandUsedAsName: return TEXT("command used as a name");
		case ECambrianaDiagnosticCode::ReservedName: return TEXT("reserved name");
		case ECambrianaDiagnosticCode::UnknownName: return TEXT("unknown name");
		case ECambrianaDiagnosticCode::DuplicateName: return TEXT("duplicate name");
		case ECambrianaDiagnosticCode::ArgumentCount: return TEXT("wrong number of arguments");
		case ECambrianaDiagnosticCode::PoseParameter: return TEXT("invalid use of a pose parameter");
		case ECambrianaDiagnosticCode::MacroCycle: return TEXT("macro cycle");
		case ECambrianaDiagnosticCode::ModuleInDrawRule: return TEXT("module in a draw rule");
		case ECambrianaDiagnosticCode::UnknownAxiom: return TEXT("unknown axiom");
		case ECambrianaDiagnosticCode::SegmentReplacedBySeveral: return TEXT("segment replaced by several");
		case ECambrianaDiagnosticCode::UnreachableRule: return TEXT("unreachable rule");
		case ECambrianaDiagnosticCode::NotConstant: return TEXT("value is not constant");
		case ECambrianaDiagnosticCode::SizeAndStep: return TEXT("both size and step are set");
		case ECambrianaDiagnosticCode::NonFiniteValue: return TEXT("value is not finite");
		case ECambrianaDiagnosticCode::NotConvertible: return TEXT("not convertible");
		case ECambrianaDiagnosticCode::ShapeParameter: return TEXT("invalid use of a shape parameter");
		default: return TEXT("unknown problem");
		}
	}

	class FCoreResult
	{
	public:
		explicit FCoreResult(cambriana_result* InResult) : Result(InResult) {}
		~FCoreResult() { cambriana_result_destroy(Result); }
		FCoreResult(const FCoreResult&) = delete;
		FCoreResult& operator=(const FCoreResult&) = delete;

		const cambriana_result* Get() const { return Result; }

	private:
		cambriana_result* Result;
	};

	template <typename T>
	TConstArrayView<T> CoreArray(const T* (*Accessor)(const cambriana_result*, uint64_t*), const cambriana_result* Result)
	{
		uint64_t Count = 0;
		const T* Data = Accessor(Result, &Count);
		return Data ? TConstArrayView<T>(Data, static_cast<int32>(FMath::Min<uint64_t>(Count, MAX_int32))) : TConstArrayView<T>();
	}

	cambriana_build_settings ToCoreSettings(const FCambrianaBuildInput& Input, ECambrianaGrammarFormat Format)
	{
		const FCambrianaBuildSettings& Settings = Input.Settings;
		cambriana_build_settings Core;
		cambriana_build_settings_init(&Core, sizeof(Core));
		Core.seed = Input.Seed;
		if (Settings.Recursion)
		{
			Core.overrides |= CAMBRIANA_OVERRIDE_RECURSION;
			Core.recursion = *Settings.Recursion;
		}
		if (Settings.BasicAngle)
		{
			Core.overrides |= CAMBRIANA_OVERRIDE_BASIC_ANGLE;
			Core.basic_angle = *Settings.BasicAngle;
		}
		if (Settings.Thickness)
		{
			Core.overrides |= CAMBRIANA_OVERRIDE_THICKNESS;
			Core.thickness = *Settings.Thickness;
		}
		Core.min_thickness = Settings.MinThickness;
		Core.closed_form = Settings.bClosedForm ? 1u : 0u;
		Core.sides = static_cast<uint64_t>(FMath::Max(Settings.Sides, 0));
		if (Format == ECambrianaGrammarFormat::Cambriana)
		{
			Core.overrides |= CAMBRIANA_OVERRIDE_SEED;
			const TPair<bool, uint64_t> Overrides[] = {
				{ Settings.bOverrideGrammarSize, CAMBRIANA_OVERRIDE_SIZE },
				{ Settings.bOverrideGrammarSides, CAMBRIANA_OVERRIDE_SIDES },
				{ Settings.bOverrideGrammarSurface, CAMBRIANA_OVERRIDE_SURFACE },
				{ Settings.bOverrideGrammarMinThickness, CAMBRIANA_OVERRIDE_MIN_THICKNESS },
			};
			for (const TPair<bool, uint64_t>& Override : Overrides)
			{
				if (Override.Key)
				{
					Core.overrides |= Override.Value;
				}
			}
		}
		if (Format == ECambrianaGrammarFormat::Legacy || Settings.bOverrideGrammarSize || Settings.bSizeForGrammarWithoutSize)
		{
			if (Settings.SizeMode == ECambrianaSizeMode::Fit)
			{
				Core.size_mode = CAMBRIANA_SIZE_FIT;
				Core.fit_size = Settings.SizeValue;
			}
			else
			{
				Core.size_mode = CAMBRIANA_SIZE_STEP;
				Core.step_length = Settings.SizeValue;
			}
		}
		Core.production_budget = Settings.ProductionBudget.Get(Core.production_budget);
		Core.bone_budget = Settings.BoneBudget.Get(Core.bone_budget);
		Core.work_budget = Settings.WorkBudget.Get(Core.work_budget);
		Core.vertex_budget = Settings.VertexBudget.Get(Core.vertex_budget);
		return Core;
	}

	void ReadReport(const cambriana_result* Result, FCambrianaBuildReport& Report)
	{
		if (const cambriana_summary* Summary = cambriana_result_summary(Result))
		{
			Cambriana::ApplyResultFlags(Summary->flags, Report);
			Report.ProductionLength = Summary->production_length;
			Report.GenerationsRun = Summary->generations_run;
			Report.Recursion = Summary->recursion;
			Report.BasicAngle = Summary->basic_angle;
			Report.Thickness = Summary->thickness;
			Report.StepLength = Summary->step_length;
		}

		for (const cambriana_diagnostic& Core : CoreArray(&cambriana_result_diagnostics, Result))
		{
			Report.Diagnostics.Add(Cambriana::ToDiagnostic(Core));
		}
	}

	ECambrianaParameterTier ToParameterTier(int32 Tier)
	{
		switch (Tier)
		{
		case CAMBRIANA_TIER_POSE: return ECambrianaParameterTier::Pose;
		case CAMBRIANA_TIER_SHAPE: return ECambrianaParameterTier::Shape;
		case CAMBRIANA_TIER_STRUCTURE: return ECambrianaParameterTier::Structure;
		default: return ECambrianaParameterTier::None;
		}
	}

	/** Cambriana grammar text as a core model; legacy text has none. */
	class FGrammarModel
	{
	public:
		FGrammarModel(const FTCHARToUTF8& Text, ECambrianaGrammarFormat Format)
		{
			if (Format == ECambrianaGrammarFormat::Cambriana)
			{
				cambriana_grammar_parse(Text.Get(), static_cast<size_t>(Text.Length()), &Grammar);
			}
		}
		~FGrammarModel() { cambriana_grammar_destroy(Grammar); }
		FGrammarModel(const FGrammarModel&) = delete;
		FGrammarModel& operator=(const FGrammarModel&) = delete;

		const cambriana_grammar* Get() const { return Grammar; }

		/** The value the grammar states for Setting, unless the caller overrides it. */
		TOptional<double> GetSetting(int32 Setting, bool bOverridden) const
		{
			double Value = 0.0;
			const bool bStated = !bOverridden && Grammar && cambriana_grammar_get_setting(Grammar, Setting, &Value) == CAMBRIANA_OK;
			return bStated ? TOptional<double>(Value) : TOptional<double>();
		}

		/** Sets each declared parameter to its stored value; the core ignores undeclared names and a null grammar. */
		void ApplyParameterValues(TConstArrayView<FCambrianaParameterValue> Values)
		{
			for (const FCambrianaParameterValue& Value : Values)
			{
				cambriana_grammar_set_parameter(Grammar, FTCHARToUTF8(*Value.Name).Get(), Value.Value);
			}
		}

		/** The declared parameters in file order, each with the model's value in Value. */
		TArray<FCambrianaParameter> ReadParameters() const
		{
			TArray<FCambrianaParameter> Parameters;
			const uint64 Count = Grammar ? cambriana_grammar_parameter_count(Grammar) : 0;
			for (uint64 Index = 0; Index < Count; ++Index)
			{
				const char* Name = nullptr;
				int32_t Tier = CAMBRIANA_TIER_NONE;
				double Value = 0.0;
				if (cambriana_grammar_parameter(Grammar, Index, &Name, &Tier, &Value) == CAMBRIANA_OK && Name)
				{
					FCambrianaParameter& Parameter = Parameters.AddDefaulted_GetRef();
					Parameter.Name = UTF8_TO_TCHAR(Name);
					Parameter.Tier = ToParameterTier(Tier);
					Parameter.Value = Value;
				}
			}
			return Parameters;
		}

	private:
		cambriana_grammar* Grammar = nullptr;
	};

	void ReadSettingsUsed(const FGrammarModel& Stated, const FCambrianaBuildSettings& Settings, const cambriana_build_settings& Core,
		FCambrianaBuildReport& Report)
	{
		int32 Mode = Core.size_mode;
		double FitSize = Core.fit_size;
		if (const TOptional<double> Size = Stated.GetSetting(CAMBRIANA_SETTING_SIZE, Settings.bOverrideGrammarSize))
		{
			Mode = CAMBRIANA_SIZE_FIT;
			FitSize = *Size;
		}
		else if (Stated.GetSetting(CAMBRIANA_SETTING_STEP, Settings.bOverrideGrammarSize))
		{
			Mode = CAMBRIANA_SIZE_STEP;
		}
		Report.SizeMode = Mode == CAMBRIANA_SIZE_FIT ? ECambrianaSizeMode::Fit : ECambrianaSizeMode::Step;
		Report.SizeValue = Mode == CAMBRIANA_SIZE_FIT ? FitSize : Report.StepLength;

		const TOptional<double> Sides = Stated.GetSetting(CAMBRIANA_SETTING_SIDES, Settings.bOverrideGrammarSides);
		Report.Sides = Sides ? FMath::RoundToInt32(*Sides) : Settings.Sides;
		const TOptional<double> Surface = Stated.GetSetting(CAMBRIANA_SETTING_SURFACE, Settings.bOverrideGrammarSurface);
		Report.bClosedForm = Surface ? *Surface != 0.0 : Settings.bClosedForm;
		Report.MinThickness = Stated.GetSetting(CAMBRIANA_SETTING_MIN_THICKNESS, Settings.bOverrideGrammarMinThickness).Get(Settings.MinThickness);
	}

	/** A legacy build with no production budget parses the header and stops before rewriting. */
	void ReadLegacyHeader(const FTCHARToUTF8& Text, FCambrianaStatedSettings& Out)
	{
		cambriana_build_settings Settings;
		cambriana_build_settings_init(&Settings, sizeof(Settings));
		Settings.production_budget = 0;
		Settings.work_budget = 0;
		cambriana_result* RawResult = nullptr;
		cambriana_build_legacy(Text.Get(), static_cast<size_t>(Text.Length()), &Settings, nullptr, &RawResult);
		const FCoreResult Result(RawResult);
		const cambriana_summary* Summary = Result.Get() ? cambriana_result_summary(Result.Get()) : nullptr;
		if (Summary && (Summary->flags & CAMBRIANA_RESULT_HAS_ERRORS) == 0)
		{
			Out.Recursion = Summary->recursion;
			Out.BasicAngle = Summary->basic_angle;
			Out.Thickness = Summary->thickness;
		}
	}

	struct FChannelScale
	{
		double MaxGeneration = 0.0;
		double MaxThickness = 0.0;
	};

	FChannelScale ScaleOf(TConstArrayView<cambriana_vertex_channels> Channels)
	{
		FChannelScale Scale;
		for (const cambriana_vertex_channels& Channel : Channels)
		{
			Scale.MaxGeneration = FMath::Max(Scale.MaxGeneration, static_cast<double>(Channel.generation));
			Scale.MaxThickness = FMath::Max(Scale.MaxThickness, Channel.thickness);
		}
		return Scale;
	}

	double Ratio(double Value, double Max)
	{
		return Max > 0.0 ? Value / Max : 0.0;
	}

	class FMeshWriter
	{
	public:
		FMeshWriter(FDynamicMesh3& InMesh, TConstArrayView<cambriana_vertex_channels> InChannels)
			: Mesh(InMesh), Channels(InChannels), Scale(ScaleOf(InChannels))
		{
			Mesh.EnableAttributes();
			UE::Geometry::FDynamicMeshAttributeSet& Attributes = *Mesh.Attributes();
			Attributes.SetNumUVLayers(3);
			Attributes.EnablePrimaryColors();
			Attributes.EnableMaterialID();
			Uv0 = Attributes.GetUVLayer(0);
			Uv1 = Attributes.GetUVLayer(1);
			Uv2 = Attributes.GetUVLayer(2);
			Colours = Attributes.PrimaryColors();
			MaterialIds = Attributes.GetMaterialID();
		}

		int32 AddVertex(const cambriana_vec3& Position, int32 CoreIndex)
		{
			const int32 Vid = Mesh.AppendVertex(FVector3d(Position.x, Position.y, Position.z));
			const cambriana_vertex_channels Channel = Channels.IsValidIndex(CoreIndex) ? Channels[CoreIndex] : cambriana_vertex_channels{};
			Uv0->AppendElement(FVector2f::ZeroVector);
			Uv1->AppendElement(FVector2f(float(Channel.along_branch), float(Ratio(double(Channel.generation), Scale.MaxGeneration))));
			Uv2->AppendElement(FVector2f(float(Channel.root_distance), float(Channel.branch_random)));
			Colours->AppendElement(FVector4f(float(Ratio(Channel.thickness, Scale.MaxThickness)), 0.0f, 0.0f, 0.0f));
			return Vid;
		}

		int32 AddTriangle(const FIndex3i& Vertices, int32 Slot)
		{
			const int32 Tid = Mesh.AppendTriangle(Vertices);
			if (Tid < 0)
			{
				return Tid;
			}
			Uv0->SetTriangle(Tid, Vertices);
			Uv1->SetTriangle(Tid, Vertices);
			Uv2->SetTriangle(Tid, Vertices);
			Colours->SetTriangle(Tid, Vertices);
			MaterialIds->SetValue(Tid, Slot);
			return Tid;
		}

	private:
		FDynamicMesh3& Mesh;
		TConstArrayView<cambriana_vertex_channels> Channels;
		FChannelScale Scale;
		UE::Geometry::FDynamicMeshUVOverlay* Uv0 = nullptr;
		UE::Geometry::FDynamicMeshUVOverlay* Uv1 = nullptr;
		UE::Geometry::FDynamicMeshUVOverlay* Uv2 = nullptr;
		UE::Geometry::FDynamicMeshColorOverlay* Colours = nullptr;
		UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = nullptr;
	};

	TArray<int64> SortedMaterialIds(TConstArrayView<cambriana_triangle> Triangles)
	{
		TArray<int64> Ids;
		for (const cambriana_triangle& Triangle : Triangles)
		{
			if (Ids.IsEmpty() || Ids.Last() != Triangle.material)
			{
				Ids.AddUnique(Triangle.material);
			}
		}
		Ids.Sort();
		return Ids;
	}

	void WriteMesh(const cambriana_result* Result, FCambrianaBuildResult& Out)
	{
		const TConstArrayView<cambriana_vec3> Positions = CoreArray(&cambriana_result_surface_positions, Result);
		const TConstArrayView<cambriana_triangle> Triangles = CoreArray(&cambriana_result_triangles, Result);
		const TConstArrayView<cambriana_vertex_channels> Channels = CoreArray(&cambriana_result_vertex_channels, Result);
		if (Triangles.IsEmpty())
		{
			return;
		}

		Out.SlotMaterialIds = SortedMaterialIds(Triangles);
		FMeshWriter Writer(Out.Mesh, Channels);
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Writer.AddVertex(Positions[Index], Index);
		}

		const uint64 PositionCount = static_cast<uint64>(Positions.Num());
		int32 Skipped = 0;
		for (const cambriana_triangle& Triangle : Triangles)
		{
			const FIndex3i Vertices(static_cast<int32>(Triangle.vertices[0]), static_cast<int32>(Triangle.vertices[1]), static_cast<int32>(Triangle.vertices[2]));
			const bool bInRange = Triangle.vertices[0] < PositionCount && Triangle.vertices[1] < PositionCount && Triangle.vertices[2] < PositionCount;
			if (!bInRange)
			{
				++Skipped;
				continue;
			}
			const int32 Slot = Algo::BinarySearch(Out.SlotMaterialIds, Triangle.material);
			int32 Tid = Writer.AddTriangle(Vertices, Slot);
			// FDynamicMesh3 rejects non-manifold edges, which core tube junctions can produce; copying the corners keeps
			// every core triangle in order while core vertex i stays mesh vertex i.
			if (Tid == FDynamicMesh3::NonManifoldID || Tid == FDynamicMesh3::DuplicateTriangleID)
			{
				FIndex3i Copies;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					Copies[Corner] = Writer.AddVertex(Positions[Vertices[Corner]], Vertices[Corner]);
				}
				Tid = Writer.AddTriangle(Copies, Slot);
			}
			Skipped += Tid < 0;
		}
		if (Skipped > 0)
		{
			UE_LOG(LogCambriana, Warning, TEXT("Skipped %d of %d core triangles that the dynamic mesh cannot hold"), Skipped, Triangles.Num());
		}

		UE::Geometry::FDynamicMeshNormalOverlay* Normals = Out.Mesh.Attributes()->PrimaryNormals();
		UE::Geometry::FMeshNormals::InitializeOverlayTopologyFromOpeningAngle(&Out.Mesh, Normals, NormalSplitAngleDegrees);
		UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(Out.Mesh);
	}
}

FString FCambrianaDiagnostic::Describe() const
{
	const TCHAR* SeverityText = Severity == ECambrianaDiagnosticSeverity::Error ? TEXT("error") : TEXT("warning");
	return FString::Printf(TEXT("%s at %s"), SeverityText, *DescribePositionAndProblem());
}

FString FCambrianaDiagnostic::DescribePositionAndProblem() const
{
	if (ProductionPosition)
	{
		return FString::Printf(TEXT("production position %llu: %s"), *ProductionPosition, DescribeCode(Code));
	}
	return FString::Printf(TEXT("line %llu, column %llu: %s"), Line, Column, DescribeCode(Code));
}

namespace Cambriana
{
	FCambrianaDiagnostic ToDiagnostic(const cambriana_diagnostic& Core)
	{
		FCambrianaDiagnostic Diagnostic;
		Diagnostic.Severity = Core.severity == CAMBRIANA_SEVERITY_ERROR ? ECambrianaDiagnosticSeverity::Error : ECambrianaDiagnosticSeverity::Warning;
		Diagnostic.Code = ToDiagnosticCode(Core.code);
		Diagnostic.Line = Core.line;
		Diagnostic.Column = Core.column;
		if (Core.production_position != CAMBRIANA_NO_POSITION)
		{
			Diagnostic.ProductionPosition = Core.production_position;
		}
		return Diagnostic;
	}

	bool ShouldBuildWithCore(bool bCoreAvailable, std::atomic<bool>& UnavailableLogged)
	{
		if (bCoreAvailable)
		{
			return true;
		}
		if (!UnavailableLogged.exchange(true))
		{
			UE_LOG(LogCambriana, Warning, TEXT("CambrianaCore is unavailable; Cambriana builds produce empty meshes"));
		}
		return false;
	}

	void ApplyResultFlags(uint32 Flags, FCambrianaBuildReport& Report)
	{
		Report.bHasErrors = (Flags & CAMBRIANA_RESULT_HAS_ERRORS) != 0;
		Report.bProductionBudgetReached = (Flags & CAMBRIANA_RESULT_PRODUCTION_BUDGET_REACHED) != 0;
		Report.bBoneBudgetReached = (Flags & CAMBRIANA_RESULT_BONE_BUDGET_REACHED) != 0;
		Report.bCancelled = (Flags & CAMBRIANA_RESULT_CANCELLED) != 0;
		Report.bWorkBudgetReached = (Flags & CAMBRIANA_RESULT_WORK_BUDGET_REACHED) != 0;
		Report.bVertexBudgetReached = (Flags & CAMBRIANA_RESULT_VERTEX_BUDGET_REACHED) != 0;
	}

	void LogBuildReport(const FCambrianaBuildReport& Report)
	{
		for (const FCambrianaDiagnostic& Diagnostic : Report.Diagnostics)
		{
			if (Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error)
			{
				UE_LOG(LogCambriana, Error, TEXT("Grammar %s"), *Diagnostic.Describe());
			}
			else
			{
				UE_LOG(LogCambriana, Warning, TEXT("Grammar %s"), *Diagnostic.Describe());
			}
		}
		const TPair<bool, const TCHAR*> Stops[] = {
			{ Report.bProductionBudgetReached, TEXT("production budget reached") },
			{ Report.bBoneBudgetReached, TEXT("bone budget reached") },
			{ Report.bWorkBudgetReached, TEXT("work budget reached") },
			{ Report.bVertexBudgetReached, TEXT("vertex budget reached") },
		};
		for (const TPair<bool, const TCHAR*>& Stop : Stops)
		{
			if (Stop.Key)
			{
				UE_LOG(LogCambriana, Warning, TEXT("Build stopped early: %s"), Stop.Value);
			}
		}
		if (Report.bCancelled)
		{
			UE_LOG(LogCambriana, Warning, TEXT("Build cancelled"));
		}
	}

	FCambrianaBuildResult BuildMesh(const FCambrianaBuildInput& Input)
	{
		FCambrianaBuildResult Out;
		if (Input.GrammarText.IsEmpty())
		{
			return Out;
		}
		static std::atomic<bool> UnavailableLogged = false;
		if (!ShouldBuildWithCore(FCambrianaModule::IsCoreAvailable(), UnavailableLogged))
		{
			Out.Report.bCoreUnavailable = true;
			return Out;
		}

		const ECambrianaGrammarFormat Format = DetectGrammarFormat(Input.GrammarText);
		const cambriana_build_settings Settings = ToCoreSettings(Input, Format);
		const FTCHARToUTF8 Text(*Input.GrammarText);
		FGrammarModel Model(Text, Format);
		cambriana_result* RawResult = nullptr;
		cambriana_status Status;
		if (Format == ECambrianaGrammarFormat::Legacy)
		{
			Status = cambriana_build_legacy(Text.Get(), static_cast<size_t>(Text.Length()), &Settings, nullptr, &RawResult);
		}
		else
		{
			Model.ApplyParameterValues(Input.ParameterValues);
			Status = cambriana_build_grammar_model(Model.Get(), nullptr, &Settings, nullptr, &RawResult);
		}
		const FCoreResult Result(RawResult);
		if (Status != CAMBRIANA_OK || !Result.Get())
		{
			UE_LOG(LogCambriana, Error, TEXT("CambrianaCore build failed with status %d"), Status);
			return Out;
		}

		ReadReport(Result.Get(), Out.Report);
		ReadSettingsUsed(Model, Input.Settings, Settings, Out.Report);
		LogBuildReport(Out.Report);
		if (!Out.Report.bHasErrors)
		{
			WriteMesh(Result.Get(), Out);
		}
		return Out;
	}

	TArray<FCambrianaParameter> ReadParameters(const FString& GrammarText, TConstArrayView<FCambrianaParameterValue> StoredValues)
	{
		const ECambrianaGrammarFormat Format = DetectGrammarFormat(GrammarText);
		if (Format != ECambrianaGrammarFormat::Cambriana || GrammarText.IsEmpty() || !FCambrianaModule::IsCoreAvailable())
		{
			return {};
		}
		const FTCHARToUTF8 Text(*GrammarText);
		FGrammarModel Applied(Text, Format);
		Applied.ApplyParameterValues(StoredValues);
		TArray<FCambrianaParameter> Parameters = Applied.ReadParameters();
		const TArray<FCambrianaParameter> InText = FGrammarModel(Text, Format).ReadParameters();
		for (int32 Index = 0; Index < Parameters.Num(); ++Index)
		{
			FCambrianaParameter& Parameter = Parameters[Index];
			Parameter.TextValue = InText.IsValidIndex(Index) ? InText[Index].Value : Parameter.Value;
			Parameter.bStored = StoredValues.ContainsByPredicate([&Parameter](const FCambrianaParameterValue& Stored)
			{
				return Stored.Name.Equals(Parameter.Name, ESearchCase::CaseSensitive);
			});
		}
		return Parameters;
	}

	FCambrianaStatedSettings ReadStatedSettings(const FString& GrammarText)
	{
		FCambrianaStatedSettings Out;
		if (GrammarText.IsEmpty() || !FCambrianaModule::IsCoreAvailable())
		{
			return Out;
		}
		const ECambrianaGrammarFormat Format = DetectGrammarFormat(GrammarText);
		const FTCHARToUTF8 Text(*GrammarText);
		if (Format == ECambrianaGrammarFormat::Legacy)
		{
			ReadLegacyHeader(Text, Out);
			return Out;
		}

		const FGrammarModel Stated(Text, Format);
		Out.Recursion = Stated.GetSetting(CAMBRIANA_SETTING_GENERATIONS, false);
		Out.BasicAngle = Stated.GetSetting(CAMBRIANA_SETTING_ANGLE, false);
		Out.Thickness = Stated.GetSetting(CAMBRIANA_SETTING_THICKNESS, false);
		if (const TOptional<double> Size = Stated.GetSetting(CAMBRIANA_SETTING_SIZE, false))
		{
			Out.SizeMode = ECambrianaSizeMode::Fit;
			Out.SizeValue = Size;
		}
		else if (const TOptional<double> Step = Stated.GetSetting(CAMBRIANA_SETTING_STEP, false))
		{
			Out.SizeMode = ECambrianaSizeMode::Step;
			Out.SizeValue = Step;
		}
		if (const TOptional<double> Sides = Stated.GetSetting(CAMBRIANA_SETTING_SIDES, false))
		{
			Out.Sides = FMath::RoundToInt32(*Sides);
		}
		if (const TOptional<double> Surface = Stated.GetSetting(CAMBRIANA_SETTING_SURFACE, false))
		{
			Out.bClosedForm = *Surface != 0.0;
		}
		Out.MinThickness = Stated.GetSetting(CAMBRIANA_SETTING_MIN_THICKNESS, false);
		return Out;
	}

	int32 PreviewPaletteIndex(int64 MaterialId)
	{
		return static_cast<int32>(((MaterialId % PreviewPaletteSize) + PreviewPaletteSize) % PreviewPaletteSize);
	}

	FLinearColor PreviewPaletteColor(int64 MaterialId)
	{
		static const FColor Palette[PreviewPaletteSize] = {
			FColor(0, 0, 0),
			FColor(165, 42, 42),
			FColor(255, 0, 0),
			FColor(255, 165, 0),
			FColor(255, 255, 0),
			FColor(0, 128, 0),
			FColor(0, 0, 255),
			FColor(238, 130, 238),
			FColor(128, 128, 128),
			FColor(255, 255, 255),
		};
		return FLinearColor::FromSRGBColor(Palette[PreviewPaletteIndex(MaterialId)]);
	}
}
