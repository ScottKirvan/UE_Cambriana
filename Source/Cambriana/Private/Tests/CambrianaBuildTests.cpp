#include "CambrianaBuild.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildEmptyUntilCoreConnectedTest,
	"Cambriana.Build.ProducesNoGeometryUntilCoreConnected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildEmptyUntilCoreConnectedTest::RunTest(const FString& Parameters)
{
	FCambrianaBuildInput Input;
	Input.GrammarText = TEXT("4\n25\n50\nA\nA=F^'A\n@\n");
	Input.Seed = 12345;
	const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
	TestEqual(TEXT("No vertices"), Result.Mesh.VertexCount(), 0);
	TestEqual(TEXT("No triangles"), Result.Mesh.TriangleCount(), 0);
	return true;
}

#endif
