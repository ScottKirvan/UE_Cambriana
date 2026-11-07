#include "AssetToolsModule.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaLiveActor.h"
#include "IAssetTools.h"
#include "IPlacementModeModule.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FString BojuStudios = TEXT("Boju Studios");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAddMenuCategoryTest,
	"Cambriana.Editor.Categories.GrammarAssetIsUnderBojuStudiosInTheAddMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAddMenuCategoryTest::RunTest(const FString& Parameters)
{
	const UCambrianaGrammarFactory* Factory = GetDefault<UCambrianaGrammarFactory>();
	if (!TestTrue(TEXT("The grammar factory appears in the Add menu"), Factory->ShouldShowInNewMenu()))
	{
		return false;
	}
	const uint32 MenuCategories = Factory->GetMenuCategories();

	TArray<FAdvancedAssetCategory> Categories;
	FAssetToolsModule::GetModule().Get().GetAllAdvancedAssetCategories(Categories);
	const FAdvancedAssetCategory* Boju = Categories.FindByPredicate([](const FAdvancedAssetCategory& Category)
	{
		return Category.CategoryName.ToString() == BojuStudios;
	});
	if (!TestNotNull(TEXT("A Boju Studios asset category is registered"), Boju))
	{
		return false;
	}
	TestEqual(TEXT("The grammar factory's Add menu categories are Boju Studios alone"), MenuCategories, static_cast<uint32>(Boju->CategoryType));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorPlacementCategoryTest,
	"Cambriana.Editor.Categories.LiveActorIsUnderBojuStudiosWhenPlacingActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorPlacementCategoryTest::RunTest(const FString& Parameters)
{
	IPlacementModeModule& Placement = IPlacementModeModule::Get();
	TArray<FPlacementCategoryInfo> Categories;
	Placement.GetSortedCategories(Categories);
	const FPlacementCategoryInfo* Boju = Categories.FindByPredicate([](const FPlacementCategoryInfo& Category)
	{
		return Category.DisplayName.ToString() == BojuStudios;
	});
	if (!TestNotNull(TEXT("A Boju Studios placement category is registered"), Boju))
	{
		return false;
	}

	TArray<TSharedPtr<FPlaceableItem>> Items;
	Placement.GetItemsForCategory(Boju->UniqueHandle, Items);
	const TSharedPtr<FPlaceableItem>* LiveActor = Items.FindByPredicate([](const TSharedPtr<FPlaceableItem>& Item)
	{
		return Item && Item->AssetData.GetAsset() == ACambrianaLiveActor::StaticClass();
	});
	TestNotNull(TEXT("The Cambriana Live Actor is placeable from Boju Studios"), LiveActor);
	return true;
}

#endif
