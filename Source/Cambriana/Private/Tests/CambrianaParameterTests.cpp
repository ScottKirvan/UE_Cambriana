#include "CambrianaBuild.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using Cambriana::Tests::TubeVertices;

namespace
{
	// One parameter of each tier, in file order; the axiom is one chain of `count` segments.
	const TCHAR* const ParameterGrammar = TEXT(
		"pose param bend = 10\n"
		"shape param girth = 1.5\n"
		"structure param count = 3\n"
		"param twice = count * 2\n"
		"\n"
		"axiom repeat count { F }\n");

	FCambrianaParameterValue Stored(const TCHAR* Name, double Value)
	{
		FCambrianaParameterValue Stored;
		Stored.Name = Name;
		Stored.Value = Value;
		return Stored;
	}

	int32 BuiltVertices(const FString& Text, TArray<FCambrianaParameterValue> Values)
	{
		FCambrianaBuildInput Input;
		Input.GrammarText = Text;
		Input.ParameterValues = MoveTemp(Values);
		return Cambriana::BuildMesh(Input).Mesh.VertexCount();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersReadTest,
	"Cambriana.Parameters.TheTextsParametersAreReadInFileOrderWithTheirTiersAndValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersReadTest::RunTest(const FString& Parameters)
{
	const TArray<FCambrianaParameter> Read = Cambriana::ReadParameters(ParameterGrammar);
	if (!TestEqual(TEXT("Four parameters"), Read.Num(), 4))
	{
		return false;
	}
	const TCHAR* Names[] = { TEXT("bend"), TEXT("girth"), TEXT("count"), TEXT("twice") };
	const ECambrianaParameterTier Tiers[] = { ECambrianaParameterTier::Pose, ECambrianaParameterTier::Shape, ECambrianaParameterTier::Structure, ECambrianaParameterTier::None };
	const double Values[] = { 10.0, 1.5, 3.0, 6.0 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Parameter %d name"), Index), Read[Index].Name, FString(Names[Index]));
		TestTrue(FString::Printf(TEXT("Parameter %d tier"), Index), Read[Index].Tier == Tiers[Index]);
		TestEqual(FString::Printf(TEXT("Parameter %d value"), Index), Read[Index].Value, Values[Index]);
		TestEqual(FString::Printf(TEXT("Parameter %d text value"), Index), Read[Index].TextValue, Values[Index]);
		TestFalse(FString::Printf(TEXT("Parameter %d has no stored value"), Index), Read[Index].bStored);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersStoredReadTest,
	"Cambriana.Parameters.AStoredValueReplacesTheTextsValueAndParametersReadingItFollow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersStoredReadTest::RunTest(const FString& Parameters)
{
	const TArray<FCambrianaParameter> Read = Cambriana::ReadParameters(ParameterGrammar, { Stored(TEXT("count"), 5.0) });
	if (!TestEqual(TEXT("Four parameters"), Read.Num(), 4))
	{
		return false;
	}
	TestTrue(TEXT("count has a stored value"), Read[2].bStored);
	TestEqual(TEXT("count's value is the stored value"), Read[2].Value, 5.0);
	TestEqual(TEXT("count's text value is still the text's"), Read[2].TextValue, 3.0);
	TestFalse(TEXT("twice has no stored value"), Read[3].bStored);
	TestEqual(TEXT("twice = count * 2 follows the stored count"), Read[3].Value, 10.0);
	TestEqual(TEXT("twice's text value reads the text's count"), Read[3].TextValue, 6.0);
	TestFalse(TEXT("bend has no stored value"), Read[0].bStored);
	TestEqual(TEXT("bend keeps the text's value"), Read[0].Value, 10.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersBuildTest,
	"Cambriana.Parameters.AStoredValueAppliesToTheBuild",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersBuildTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("The text's count of 3 gives 3 segments at 8 sides"), BuiltVertices(ParameterGrammar, {}), TubeVertices(3, 8));
	TestEqual(TEXT("A stored count of 5 gives 5 segments at 8 sides"), BuiltVertices(ParameterGrammar, { Stored(TEXT("count"), 5.0) }), TubeVertices(5, 8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersUndeclaredTest,
	"Cambriana.Parameters.StoredValuesForNamesTheTextDoesNotDeclareAreIgnored",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersUndeclaredTest::RunTest(const FString& Parameters)
{
	const TArray<FCambrianaParameterValue> Values = { Stored(TEXT("missing"), 9.0), Stored(TEXT("Count"), 5.0) };
	FCambrianaBuildInput Input;
	Input.GrammarText = ParameterGrammar;
	Input.ParameterValues = Values;
	const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
	TestFalse(TEXT("The build reports no errors"), Result.Report.bHasErrors);
	TestEqual(TEXT("Names are case-sensitive: the text's count of 3 still applies"), Result.Mesh.VertexCount(), TubeVertices(3, 8));

	const TArray<FCambrianaParameter> Read = Cambriana::ReadParameters(ParameterGrammar, Values);
	TestEqual(TEXT("Only the text's four parameters are read"), Read.Num(), 4);
	for (const FCambrianaParameter& Parameter : Read)
	{
		TestFalse(FString::Printf(TEXT("%s has no stored value"), *Parameter.Name), Parameter.bStored);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersInputEqualityTest,
	"Cambriana.Parameters.BuildInputsDifferingOnlyInAParameterValueOrNameCaseDiffer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersInputEqualityTest::RunTest(const FString& Parameters)
{
	FCambrianaBuildInput A;
	A.GrammarText = ParameterGrammar;
	A.ParameterValues = { Stored(TEXT("count"), 5.0) };
	FCambrianaBuildInput B = A;
	TestTrue(TEXT("Equal inputs compare equal"), A == B);
	B.ParameterValues[0].Value = 6.0;
	TestFalse(TEXT("A different value differs"), A == B);
	B.ParameterValues[0] = Stored(TEXT("Count"), 5.0);
	TestFalse(TEXT("A name differing only in case differs"), A == B);
	B.ParameterValues.Reset();
	TestFalse(TEXT("No stored value differs"), A == B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersLegacyTest,
	"Cambriana.Parameters.LegacyTextHasNoParametersAndIgnoresStoredValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersLegacyTest::RunTest(const FString& Parameters)
{
	const FString Legacy = Cambriana::Tests::SimpleGrammar(3);
	TestEqual(TEXT("Legacy text has no parameters"), Cambriana::ReadParameters(Legacy, { Stored(TEXT("count"), 5.0) }).Num(), 0);
	TestEqual(TEXT("A stored value leaves the legacy build alone"), BuiltVertices(Legacy, { Stored(TEXT("count"), 5.0) }), BuiltVertices(Legacy, {}));
	TestEqual(TEXT("The legacy text builds 3 segments at 8 sides"), BuiltVertices(Legacy, {}), TubeVertices(3, 8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaParametersLiveActorTest,
	"Cambriana.Parameters.ALiveActorReferencingTheAssetBuildsWithItsParameterValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaParametersLiveActorTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Referencing = World.Get()->SpawnActor<ACambrianaLiveActor>();
	ACambrianaLiveActor* Inline = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Referencing actor spawned"), Referencing) || !TestNotNull(TEXT("Inline actor spawned"), Inline))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
	Grammar->GrammarText = ParameterGrammar;
	Grammar->ParameterValues = { Stored(TEXT("count"), 5.0) };

	Referencing->SetGrammar(Grammar);
	TestTrue(TEXT("The actor's build input holds the asset's parameter values"), Referencing->GetLastBuildInput().ParameterValues == Grammar->ParameterValues);
	TestTrue(TEXT("The preview's build input holds them too"), ACambrianaLiveActor::MakeBuildInputFor(*Grammar).ParameterValues == Grammar->ParameterValues);
	TestEqual(TEXT("The asset's count of 5 gives 5 segments at 8 sides"),
		Referencing->GetDynamicMeshComponent()->GetMesh()->VertexCount(), TubeVertices(5, 8));

	Inline->InlineGrammar = ParameterGrammar;
	Inline->Rebuild();
	TestEqual(TEXT("Inline text has no stored values"), Inline->GetLastBuildInput().ParameterValues.Num(), 0);
	TestEqual(TEXT("Inline text builds with the text's count of 3"), Inline->GetDynamicMeshComponent()->GetMesh()->VertexCount(), TubeVertices(3, 8));
	return true;
}

#endif
