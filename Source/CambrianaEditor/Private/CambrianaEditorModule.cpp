#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaSeedCustomization.h"
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
	}

	virtual void ShutdownModule() override
	{
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
	void RegisterSeedCustomization(FPropertyEditorModule& PropertyEditor, UClass* Class, FName SeedPropertyName)
	{
		PropertyEditor.RegisterCustomClassLayout(Class->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FCambrianaSeedCustomization::MakeInstance, SeedPropertyName, Class));
		CustomizedClasses.Add(Class->GetFName());
	}

	TArray<FName> CustomizedClasses;
};

IMPLEMENT_MODULE(FCambrianaEditorModule, CambrianaEditor)
