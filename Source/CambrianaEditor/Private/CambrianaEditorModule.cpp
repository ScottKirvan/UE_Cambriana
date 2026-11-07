#include "CambrianaEditorCategory.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaSeedCustomization.h"
#include "IPlacementModeModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

class FCambrianaEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		RegisterSeedCustomization(PropertyEditor, UCambrianaGrammar::StaticClass(), GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Seed));
		RegisterSeedCustomization(PropertyEditor, ACambrianaLiveActor::StaticClass(), GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Seed));
		RegisterPlacementCategory();
	}

	virtual void ShutdownModule() override
	{
		if (IPlacementModeModule::IsAvailable())
		{
			IPlacementModeModule::Get().UnregisterPlacementCategory(Cambriana::Editor::BojuStudiosCategoryName);
		}
		if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			for (const FName& ClassName : CustomizedClasses)
			{
				PropertyEditor->UnregisterCustomClassLayout(ClassName);
			}
		}
		CustomizedClasses.Reset();
	}

private:
	static void RegisterPlacementCategory()
	{
		IPlacementModeModule& Placement = IPlacementModeModule::Get();
		const FName Category = Cambriana::Editor::BojuStudiosCategoryName;
		Placement.RegisterPlacementCategory(FPlacementCategoryInfo(Cambriana::Editor::BojuStudiosCategoryText(), Category, TEXT("PMBojuStudios")));
		Placement.RegisterPlaceableItem(Category, MakeShared<FPlaceableItem>(static_cast<UActorFactory*>(nullptr),FAssetData(ACambrianaLiveActor::StaticClass())));
	}

	void RegisterSeedCustomization(FPropertyEditorModule& PropertyEditor, UClass* Class, FName SeedPropertyName)
	{
		PropertyEditor.RegisterCustomClassLayout(Class->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FCambrianaSeedCustomization::MakeInstance, SeedPropertyName, Class));
		CustomizedClasses.Add(Class->GetFName());
	}

	TArray<FName> CustomizedClasses;
};

IMPLEMENT_MODULE(FCambrianaEditorModule, CambrianaEditor)
