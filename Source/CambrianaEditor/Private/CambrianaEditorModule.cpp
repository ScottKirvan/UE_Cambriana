#include "CambrianaEditorCategory.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarCustomization.h"
#include "CambrianaLiveActor.h"
#include "CambrianaLiveActorCustomization.h"
#include "Engine/Engine.h"
#include "IPlacementModeModule.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

class FCambrianaEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		RegisterCustomization(PropertyEditor, UCambrianaGrammar::StaticClass(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FCambrianaGrammarCustomization::MakeInstance));
		RegisterCustomization(PropertyEditor, ACambrianaLiveActor::StaticClass(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FCambrianaLiveActorCustomization::MakeInstance));
		// Placement mode needs the editor, which isn't up yet while plugin modules start at their usual loading phase.
		if (GEngine && GEngine->IsInitialized())
		{
			RegisterPlacementCategory();
		}
		else
		{
			FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FCambrianaEditorModule::RegisterPlacementCategory);
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);
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
	void RegisterPlacementCategory()
	{
		if (!GIsEditor || IsRunningCommandlet())
		{
			return;
		}
		IPlacementModeModule& Placement = IPlacementModeModule::Get();
		const FName Category = Cambriana::Editor::BojuStudiosCategoryName;
		Placement.RegisterPlacementCategory(FPlacementCategoryInfo(Cambriana::Editor::BojuStudiosCategoryText(), Category, TEXT("PMBojuStudios")));
		Placement.RegisterPlaceableItem(Category, MakeShared<FPlaceableItem>(static_cast<UActorFactory*>(nullptr),FAssetData(ACambrianaLiveActor::StaticClass())));
	}

	void RegisterCustomization(FPropertyEditorModule& PropertyEditor, UClass* Class, FOnGetDetailCustomizationInstance MakeInstance)
	{
		PropertyEditor.RegisterCustomClassLayout(Class->GetFName(), MoveTemp(MakeInstance));
		CustomizedClasses.Add(Class->GetFName());
	}

	TArray<FName> CustomizedClasses;
};

IMPLEMENT_MODULE(FCambrianaEditorModule, CambrianaEditor)
