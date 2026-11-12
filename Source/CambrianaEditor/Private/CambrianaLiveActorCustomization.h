#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IPropertyHandle;

namespace Cambriana::Editor
{
	/** Whether any live actor behind the handle differs from the value its setting starts from. */
	bool DiffersFromStartingValue(const IPropertyHandle& SettingHandle);

	/** Sets each live actor behind the handle to the value its setting starts from, as one undoable edit. */
	void ResetToStartingValues(IPropertyHandle& SettingHandle);
}

/** Live actor Details: the seed field with a new-seed button, and each overridable setting's reset arrow going to its starting value. */
class FCambrianaLiveActorCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
