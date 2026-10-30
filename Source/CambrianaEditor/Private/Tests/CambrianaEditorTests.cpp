#include "ActorFactoryCambrianaGrammar.h"
#include "AssetSelection.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaLiveActor.h"
#include "CambrianaSeedCustomization.h"
#include "Editor.h"
#include "Elements/Actor/ActorElementData.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Subsystems/PlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UCambrianaGrammar* CreateGrammarWithFactory()
	{
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		return Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass()), RF_Transactional, nullptr, nullptr));
	}

	TSharedPtr<IPropertyHandle> FindPropertyHandle(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, FName PropertyName)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() && Handle->GetProperty()->GetFName() == PropertyName)
			{
				return Handle;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			if (TSharedPtr<IPropertyHandle> Found = FindPropertyHandle(Children, PropertyName))
			{
				return Found;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarFactoryNewAssetTest,
	"Cambriana.Editor.GrammarFactory.NewAssetHasDefaultGrammarAndGeneratedSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarFactoryNewAssetTest::RunTest(const FString& Parameters)
{
	const UCambrianaGrammarFactory* Factory = GetDefault<UCambrianaGrammarFactory>();
	TestTrue(TEXT("Factory creates new assets from the Content Browser"), Factory->CanCreateNew());
	TestTrue(TEXT("Factory creates grammar assets"), Factory->GetSupportedClass() == UCambrianaGrammar::StaticClass());

	const UCambrianaGrammar* First = CreateGrammarWithFactory();
	const UCambrianaGrammar* Second = CreateGrammarWithFactory();
	if (!TestNotNull(TEXT("First asset created"), First) || !TestNotNull(TEXT("Second asset created"), Second))
	{
		return false;
	}
	TestEqual(TEXT("New asset holds the default legacy grammar"), First->GrammarText, FString(UCambrianaGrammarFactory::DefaultGrammarText));
	TestNotEqual(TEXT("Two new assets get different seeds"), First->Seed, Second->Seed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaDragGrammarIntoLevelTest,
	"Cambriana.Editor.DraggingGrammarIntoLevelPlacesReferencingLiveActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaDragGrammarIntoLevelTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* Grammar = CreateGrammarWithFactory();
	if (!TestNotNull(TEXT("Grammar created"), Grammar))
	{
		return false;
	}

	UPlacementSubsystem* Placement = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
	if (!TestNotNull(TEXT("Placement subsystem available"), Placement))
	{
		return false;
	}
	const TScriptInterface<IAssetFactoryInterface> AssetFactory = Placement->FindAssetFactoryFromAssetData(FAssetData(Grammar));
	TestTrue(TEXT("The live actor factory handles grammar assets"), Cast<UActorFactoryCambrianaGrammar>(AssetFactory.GetObject()) != nullptr);

	UE::AssetPlacementUtil::FExtraPlaceAssetOptions Options;
	Options.bSelectOutput = false;
	const TArray<FTypedElementHandle> Placed = UE::AssetPlacementUtil::PlaceAssetInCurrentLevel(Grammar, Options);
	if (!TestEqual(TEXT("One element placed"), Placed.Num(), 1))
	{
		return false;
	}

	ACambrianaLiveActor* Actor = Cast<ACambrianaLiveActor>(ActorElementDataUtil::GetActorFromHandle(Placed[0]));
	if (TestNotNull(TEXT("The placed element is a live actor"), Actor))
	{
		TestTrue(TEXT("The live actor references the dropped grammar"), Actor->Grammar == Grammar);
		TestEqual(TEXT("The live actor was built from the grammar's text"), Actor->GetLastBuildInput().GrammarText, Grammar->GrammarText);
		Actor->GetWorld()->EditorDestroyActor(Actor, false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaNewSeedButtonTest,
	"Cambriana.Editor.NewSeedGivesEachSelectedObjectANewSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaNewSeedButtonTest::RunTest(const FString& Parameters)
{
	UCambrianaGrammar* First = CreateGrammarWithFactory();
	UCambrianaGrammar* Second = CreateGrammarWithFactory();
	if (!TestNotNull(TEXT("First asset created"), First) || !TestNotNull(TEXT("Second asset created"), Second))
	{
		return false;
	}
	First->Seed = 1;
	Second->Seed = 1;

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const TSharedRef<IPropertyRowGenerator> Rows = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
	Rows->SetObjects({ First, Second });
	const TSharedPtr<IPropertyHandle> SeedHandle = FindPropertyHandle(Rows->GetRootTreeNodes(), GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed));
	if (!TestTrue(TEXT("Seed property found"), SeedHandle.IsValid()))
	{
		return false;
	}

	Cambriana::Editor::AssignNewSeeds(*SeedHandle);

	TestNotEqual(TEXT("First seed changed"), First->Seed, 1u);
	TestNotEqual(TEXT("Second seed changed"), Second->Seed, 1u);
	TestNotEqual(TEXT("Each object got its own seed"), First->Seed, Second->Seed);
	return true;
}

#endif
