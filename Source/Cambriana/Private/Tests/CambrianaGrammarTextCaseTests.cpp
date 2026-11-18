#include "CambrianaBuild.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 TestSides = 8;
	const TCHAR* TwoDrawnSegments = TEXT("1\n25\n50\nA\nA=FF\n@\n");
	const TCHAR* OneDrawnSegment = TEXT("1\n25\n50\nA\nA=Ff\n@\n");

	int32 VertexCountOf(const ACambrianaLiveActor* Actor)
	{
		return Actor->GetDynamicMeshComponent()->GetMesh()->VertexCount();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBuildInputTextCaseTest,
	"Cambriana.Build.InputsDifferingOnlyInGrammarTextCaseAreUnequal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaBuildInputTextCaseTest::RunTest(const FString& Parameters)
{
	FCambrianaBuildInput Upper;
	Upper.GrammarText = TwoDrawnSegments;
	FCambrianaBuildInput SameText = Upper;
	FCambrianaBuildInput Lower = Upper;
	Lower.GrammarText = OneDrawnSegment;

	TestTrue(TEXT("Inputs with the same text are equal"), Upper == SameText);
	TestFalse(TEXT("Inputs whose text differs only in case are not equal"), Upper == Lower);
	TestTrue(TEXT("Inputs whose text differs only in case compare unequal"), Upper != Lower);
	return true;
}

#if WITH_EDITOR

using Cambriana::Tests::EditProperty;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorTextCaseRebuildTest,
	"Cambriana.LiveActor.GrammarTextChangeOnlyInCaseRebuildsActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorTextCaseRebuildTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	ACambrianaLiveActor* InlineActor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	ACambrianaLiveActor* AssetActor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("Inline actor spawned"), InlineActor) || !TestNotNull(TEXT("Asset actor spawned"), AssetActor))
	{
		return false;
	}

	EditProperty(InlineActor, GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides), [InlineActor] { InlineActor->Sides = TestSides; });
	EditProperty(InlineActor, GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, InlineGrammar), [InlineActor] { InlineActor->InlineGrammar = TwoDrawnSegments; });
	TestEqual(TEXT("Inline FF: one tube of two segments"), VertexCountOf(InlineActor), 3 * TestSides);
	const int32 InlineBefore = InlineActor->GetBuildCount();

	EditProperty(InlineActor, GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, InlineGrammar), [InlineActor] { InlineActor->InlineGrammar = OneDrawnSegment; });
	TestEqual(TEXT("A case-only inline text change rebuilds once"), InlineActor->GetBuildCount(), InlineBefore + 1);
	TestEqual(TEXT("Inline Ff: one tube of one segment"), VertexCountOf(InlineActor), 2 * TestSides);

	UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
	Grammar->GrammarText = TwoDrawnSegments;
	Grammar->Sides = TestSides;
	AssetActor->SetGrammar(Grammar);
	TestEqual(TEXT("Asset FF: one tube of two segments"), VertexCountOf(AssetActor), 3 * TestSides);
	const int32 AssetBefore = AssetActor->GetBuildCount();

	EditProperty(Grammar, GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText), [Grammar] { Grammar->GrammarText = OneDrawnSegment; });
	TestEqual(TEXT("A case-only asset text change rebuilds once"), AssetActor->GetBuildCount(), AssetBefore + 1);
	TestEqual(TEXT("Asset Ff: one tube of one segment"), VertexCountOf(AssetActor), 2 * TestSides);
	return true;
}

#endif

#endif
