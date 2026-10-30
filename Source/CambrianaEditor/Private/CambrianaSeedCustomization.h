#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IPropertyHandle;

namespace Cambriana::Editor
{
	/** Gives each object behind the handle its own newly generated seed. */
	void AssignNewSeeds(IPropertyHandle& SeedHandle);
}

/** Shows a seed property as an editable number field with a new-seed button. */
class FCambrianaSeedCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(FName SeedPropertyName, UClass* OwnerClass);

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FCambrianaSeedCustomization(FName InSeedPropertyName, UClass* InOwnerClass);

	FName SeedPropertyName;
	UClass* OwnerClass;
};
