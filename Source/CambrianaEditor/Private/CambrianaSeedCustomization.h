#pragma once

#include "CoreMinimal.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class UClass;

namespace Cambriana::Editor
{
	/** Gives each object behind the handle its own newly generated seed. */
	void AssignNewSeeds(IPropertyHandle& SeedHandle);

	/** Shows OwnerClass's seed property as an editable number field with a new-seed button. */
	void CustomizeSeedRow(IDetailLayoutBuilder& DetailBuilder, FName SeedPropertyName, UClass* OwnerClass);
}
