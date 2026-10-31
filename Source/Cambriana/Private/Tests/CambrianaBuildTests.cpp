#include "CambrianaBuild.h"
#include "CambrianaBuildInternal.h"
#include "CambrianaTestUtilities.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::LoadFixture;
using Cambriana::Tests::SimpleGrammar;

namespace
{
	FCambrianaBuildInput SkInput()
	{
		FCambrianaBuildInput Input;
		Input.GrammarText = LoadFixture(TEXT("sk.ls"));
		Input.Seed = 12345;
		return Input;
	}

	double LargestDimension(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		return Mesh.GetBounds().MaxDim();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildSkTubeCountsTest,
	"Cambriana.Build.SkLsBuildsSpecifiedTubeCounts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildSkTubeCountsTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildInput Input = SkInput();
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Input.GrammarText.IsEmpty()))
	{
		return false;
	}
	const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
	TestFalse(TEXT("No errors"), Result.Report.bHasErrors);
	TestEqual(TEXT("No diagnostics"), Result.Report.Diagnostics.Num(), 0);
	TestEqual(TEXT("Production length"), Result.Report.ProductionLength, uint64(256));
	TestEqual(TEXT("Vertices"), Result.Mesh.VertexCount(), 672);
	TestEqual(TEXT("Triangles"), Result.Mesh.TriangleCount(), 1280);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildEmptyTextTest,
	"Cambriana.Build.EmptyGrammarTextBuildsNothingSilently",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildEmptyTextTest::RunTest(const FString& Parameters)
{
	const FCambrianaBuildResult Result = Cambriana::BuildMesh(FCambrianaBuildInput());
	TestEqual(TEXT("No vertices"), Result.Mesh.VertexCount(), 0);
	TestEqual(TEXT("No diagnostics"), Result.Report.Diagnostics.Num(), 0);
	TestFalse(TEXT("No errors"), Result.Report.bHasErrors);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildErrorGrammarTest,
	"Cambriana.Build.GrammarWithErrorsBuildsEmptyMeshAndReportsErrors",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildErrorGrammarTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("Grammar error at line"), ELogVerbosity::Error);
	FCambrianaBuildInput Input;
	Input.GrammarText = TEXT("20\n25\n50\n");
	const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
	TestTrue(TEXT("Report has errors"), Result.Report.bHasErrors);
	TestEqual(TEXT("No vertices"), Result.Mesh.VertexCount(), 0);
	TestEqual(TEXT("No triangles"), Result.Mesh.TriangleCount(), 0);
	TestEqual(TEXT("No material slots"), Result.SlotMaterialIds.Num(), 0);
	if (TestEqual(TEXT("One diagnostic"), Result.Report.Diagnostics.Num(), 1))
	{
		const FCambrianaDiagnostic& Diagnostic = Result.Report.Diagnostics[0];
		TestTrue(TEXT("Diagnostic is an error"), Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error);
		TestTrue(TEXT("Diagnostic is a missing axiom"), Diagnostic.Code == ECambrianaDiagnosticCode::MissingAxiom);
		TestTrue(TEXT("Diagnostic has a line"), Diagnostic.Line > 0);
		TestTrue(TEXT("Diagnostic has a column"), Diagnostic.Column > 0);
		TestTrue(TEXT("Description names the problem"), Diagnostic.Describe().Contains(TEXT("missing axiom")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildWarningGrammarTest,
	"Cambriana.Build.GrammarWithWarningsBuildsAndReportsLineAndColumn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildWarningGrammarTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("Grammar warning at line 2, column 1: invalid number"), ELogVerbosity::Warning);
	FCambrianaBuildInput Input;
	Input.GrammarText = TEXT("4\nabc\n50\nA\nA=F^'A\n@\n");
	const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
	TestFalse(TEXT("Report has no errors"), Result.Report.bHasErrors);
	TestTrue(TEXT("Mesh has geometry"), Result.Mesh.TriangleCount() > 0);
	if (TestEqual(TEXT("One diagnostic"), Result.Report.Diagnostics.Num(), 1))
	{
		const FCambrianaDiagnostic& Diagnostic = Result.Report.Diagnostics[0];
		TestTrue(TEXT("Diagnostic is a warning"), Diagnostic.Severity == ECambrianaDiagnosticSeverity::Warning);
		TestTrue(TEXT("Diagnostic is an invalid number"), Diagnostic.Code == ECambrianaDiagnosticCode::InvalidNumber);
		TestEqual(TEXT("Line"), Diagnostic.Line, uint64(2));
		TestEqual(TEXT("Column"), Diagnostic.Column, uint64(1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildBudgetsTest,
	"Cambriana.Build.BudgetsStopTheBuildAndAreReported",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildBudgetsTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("production budget reached"), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("bone budget reached"), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("work budget reached"), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("vertex budget reached"), ELogVerbosity::Warning);

	const FCambrianaBuildResult Unlimited = Cambriana::BuildMesh(SkInput());
	TestFalse(TEXT("Default budgets are not reached by sk.ls"),
		Unlimited.Report.bProductionBudgetReached || Unlimited.Report.bBoneBudgetReached
		|| Unlimited.Report.bWorkBudgetReached || Unlimited.Report.bVertexBudgetReached);

	FCambrianaBuildInput Production = SkInput();
	Production.Settings.ProductionBudget = 20;
	const FCambrianaBuildResult ProductionResult = Cambriana::BuildMesh(Production);
	TestTrue(TEXT("Production budget reached"), ProductionResult.Report.bProductionBudgetReached);
	TestTrue(TEXT("Production limited"), ProductionResult.Report.ProductionLength <= 20);

	FCambrianaBuildInput Bones = SkInput();
	Bones.Settings.BoneBudget = 5;
	const FCambrianaBuildResult BoneResult = Cambriana::BuildMesh(Bones);
	TestTrue(TEXT("Bone budget reached"), BoneResult.Report.bBoneBudgetReached);
	TestTrue(TEXT("Bone budget gives a smaller mesh"), BoneResult.Mesh.VertexCount() < Unlimited.Mesh.VertexCount());

	FCambrianaBuildInput Work = SkInput();
	Work.Settings.WorkBudget = 100;
	const FCambrianaBuildResult WorkResult = Cambriana::BuildMesh(Work);
	TestTrue(TEXT("Work budget reached"), WorkResult.Report.bWorkBudgetReached);
	TestTrue(TEXT("Work budget stops rewriting early"), WorkResult.Report.GenerationsRun < 20);

	FCambrianaBuildInput Vertices = SkInput();
	Vertices.Settings.VertexBudget = 100;
	const FCambrianaBuildResult VertexResult = Cambriana::BuildMesh(Vertices);
	TestTrue(TEXT("Vertex budget reached"), VertexResult.Report.bVertexBudgetReached);
	TestTrue(TEXT("Vertex budget caps vertices"), VertexResult.Mesh.VertexCount() <= 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildResultFlagsTest,
	"Cambriana.Build.CoreResultFlagsMapToReport",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildResultFlagsTest::RunTest(const FString& Parameters)
{
	const auto Report = [](uint32 Flags)
	{
		FCambrianaBuildReport Result;
		Cambriana::ApplyResultFlags(Flags, Result);
		return Result;
	};
	const FCambrianaBuildReport None = Report(0);
	TestFalse(TEXT("No flags set"), None.bHasErrors || None.bProductionBudgetReached || None.bBoneBudgetReached
		|| None.bWorkBudgetReached || None.bVertexBudgetReached || None.bCancelled);
	TestTrue(TEXT("Errors"), Report(CAMBRIANA_RESULT_HAS_ERRORS).bHasErrors);
	TestTrue(TEXT("Production budget"), Report(CAMBRIANA_RESULT_PRODUCTION_BUDGET_REACHED).bProductionBudgetReached);
	TestTrue(TEXT("Bone budget"), Report(CAMBRIANA_RESULT_BONE_BUDGET_REACHED).bBoneBudgetReached);
	TestTrue(TEXT("Work budget"), Report(CAMBRIANA_RESULT_WORK_BUDGET_REACHED).bWorkBudgetReached);
	TestTrue(TEXT("Vertex budget"), Report(CAMBRIANA_RESULT_VERTEX_BUDGET_REACHED).bVertexBudgetReached);
	const FCambrianaBuildReport Cancelled = Report(CAMBRIANA_RESULT_CANCELLED);
	TestTrue(TEXT("Cancelled"), Cancelled.bCancelled);
	TestFalse(TEXT("Cancelled alone sets nothing else"), Cancelled.bHasErrors || Cancelled.bWorkBudgetReached);

	AddExpectedMessagePlain(TEXT("Build cancelled"), ELogVerbosity::Warning);
	Cambriana::LogBuildReport(Cancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildCoreUnavailableTest,
	"Cambriana.Build.UnavailableCoreIsReportedOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildCoreUnavailableTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("CambrianaCore is unavailable"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	std::atomic<bool> Logged = false;
	TestFalse(TEXT("First build without the core is refused"), Cambriana::ShouldBuildWithCore(false, Logged));
	TestFalse(TEXT("Second build without the core is refused"), Cambriana::ShouldBuildWithCore(false, Logged));
	TestTrue(TEXT("Builds with the core proceed"), Cambriana::ShouldBuildWithCore(true, Logged));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildFitSizeTest,
	"Cambriana.Build.FitModeScalesLargestDimensionToTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildFitSizeTest::RunTest(const FString& Parameters)
{
	for (const double Target : { 200.0, 350.0 })
	{
		FCambrianaBuildInput Input = SkInput();
		Input.Settings.SizeMode = ECambrianaSizeMode::Fit;
		Input.Settings.SizeValue = Target;
		const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
		TestNearlyEqual(FString::Printf(TEXT("Largest dimension at target %g"), Target), LargestDimension(Result.Mesh), Target, 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildStepSizeTest,
	"Cambriana.Build.StepModeUsesSizeValueAsStepLength",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildStepSizeTest::RunTest(const FString& Parameters)
{
	FCambrianaBuildInput Short = SkInput();
	Short.Settings.SizeMode = ECambrianaSizeMode::Step;
	Short.Settings.SizeValue = 10.0;
	FCambrianaBuildInput Long = Short;
	Long.Settings.SizeValue = 20.0;

	const FCambrianaBuildResult ShortResult = Cambriana::BuildMesh(Short);
	const FCambrianaBuildResult LongResult = Cambriana::BuildMesh(Long);
	TestEqual(TEXT("Step length reported"), ShortResult.Report.StepLength, 10.0);
	TestEqual(TEXT("Doubled step length reported"), LongResult.Report.StepLength, 20.0);
	TestNearlyEqual(TEXT("Doubling the step doubles the size"), LargestDimension(LongResult.Mesh), 2.0 * LargestDimension(ShortResult.Mesh), 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaPreviewPaletteTest,
	"Cambriana.Build.PreviewPaletteRepeatsEveryTenIds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaPreviewPaletteTest::RunTest(const FString& Parameters)
{
	const FColor Expected[] = {
		FColor(0, 0, 0), FColor(165, 42, 42), FColor(255, 0, 0), FColor(255, 165, 0), FColor(255, 255, 0),
		FColor(0, 128, 0), FColor(0, 0, 255), FColor(238, 130, 238), FColor(128, 128, 128), FColor(255, 255, 255),
	};
	for (int64 Id = 0; Id < 30; ++Id)
	{
		TestEqual(FString::Printf(TEXT("Index of ID %lld"), Id), Cambriana::PreviewPaletteIndex(Id), static_cast<int32>(Id % 10));
		TestEqual(FString::Printf(TEXT("Colour of ID %lld"), Id), Cambriana::PreviewPaletteColor(Id).ToFColorSRGB(), Expected[Id % 10]);
	}
	TestEqual(TEXT("Negative IDs wrap upwards"), Cambriana::PreviewPaletteIndex(-1), 9);
	TestEqual(TEXT("Negative multiples of ten wrap to zero"), Cambriana::PreviewPaletteIndex(-20), 0);
	return true;
}

#endif
