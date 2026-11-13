#include "CambrianaBuild.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarConversion.h"
#include "CambrianaLiveActor.h"
#include "CambrianaModule.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The runtime module shut down, as when the core is missing; started again on scope exit. */
	class FRuntimeModuleShutDown
	{
	public:
		FRuntimeModuleShutDown()
			: Module(FModuleManager::Get().GetModule(TEXT("Cambriana")))
		{
			if (Module)
			{
				Module->ShutdownModule();
			}
		}

		~FRuntimeModuleShutDown()
		{
			if (Module)
			{
				Module->StartupModule();
			}
		}

		FRuntimeModuleShutDown(const FRuntimeModuleShutDown&) = delete;
		FRuntimeModuleShutDown& operator=(const FRuntimeModuleShutDown&) = delete;

		bool IsValid() const { return Module != nullptr; }

	private:
		IModuleInterface* Module;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaCoreUnavailableBuildTest,
	"Cambriana.Build.WithoutTheCoreConversionAndBuildReportItAndProduceNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaCoreUnavailableBuildTest::RunTest(const FString& Parameters)
{
	{
		const FRuntimeModuleShutDown Down;
		if (!TestTrue(TEXT("The runtime module is loaded"), Down.IsValid()))
		{
			return false;
		}
		TestFalse(TEXT("The core is unavailable"), FCambrianaModule::IsCoreAvailable());

		const FCambrianaLegacyConversion Conversion = Cambriana::ConvertLegacyGrammar(Cambriana::Tests::SimpleGrammar(2));
		TestTrue(TEXT("Conversion reports the core unavailable"), Conversion.bCoreUnavailable);
		TestFalse(TEXT("Conversion gives no text"), Conversion.GrammarText.IsSet());
		TestEqual(TEXT("Conversion gives no diagnostics"), Conversion.Diagnostics.Num(), 0);

		FCambrianaBuildInput Input;
		Input.GrammarText = Cambriana::Tests::SimpleGrammar(2);
		const FCambrianaBuildResult Result = Cambriana::BuildMesh(Input);
		TestTrue(TEXT("The build report says the core is unavailable"), Result.Report.bCoreUnavailable);
		TestEqual(TEXT("The build makes no vertices"), Result.Mesh.VertexCount(), 0);
		TestFalse(TEXT("The unavailable core is not a grammar error"), Result.Report.bHasErrors);
	}
	TestTrue(TEXT("The core is available again"), FCambrianaModule::IsCoreAvailable());
	const FCambrianaLegacyConversion Conversion = Cambriana::ConvertLegacyGrammar(Cambriana::Tests::SimpleGrammar(2));
	TestFalse(TEXT("Conversion works again"), Conversion.bCoreUnavailable);
	TestTrue(TEXT("Conversion gives text again"), Conversion.GrammarText.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaModuleShutdownGrammarChangedTest,
	"Cambriana.LiveActor.AGrammarChangeDoesNotRebuildActorsOnceTheModuleIsShutDownAndDoesAgainAfterItStarts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaModuleShutdownGrammarChangedTest::RunTest(const FString& Parameters)
{
	FCambrianaTestWorld World;
	UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
	Grammar->GrammarText = Cambriana::Tests::SimpleGrammar(2);
	ACambrianaLiveActor* Actor = World.Get()->SpawnActor<ACambrianaLiveActor>();
	if (!TestNotNull(TEXT("A live actor spawns"), Actor))
	{
		return false;
	}
	Actor->SetGrammar(Grammar);
	Grammar->GrammarText = Cambriana::Tests::SimpleGrammar(3);
	const int32 BuildsBefore = Actor->GetBuildCount();
	{
		const FRuntimeModuleShutDown Down;
		if (!TestTrue(TEXT("The runtime module is loaded"), Down.IsValid()))
		{
			return false;
		}
		UCambrianaGrammar::OnGrammarChanged().Broadcast(Grammar);
		TestEqual(TEXT("A change while the module is shut down does not rebuild the actor"), Actor->GetBuildCount(), BuildsBefore);
	}
	UCambrianaGrammar::OnGrammarChanged().Broadcast(Grammar);
	TestEqual(TEXT("A change after the module starts again rebuilds the actor once"), Actor->GetBuildCount(), BuildsBefore + 1);
	return true;
}

#endif
