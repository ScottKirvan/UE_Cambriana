#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IPropertyHandle;

namespace Cambriana::Editor
{
	/** Whether the build of any live actor behind the handle uses the actor's own value of its setting. */
	bool UsesOwnValue(const IPropertyHandle& SettingHandle);

	/** Whether any live actor behind the handle differs from the value its setting starts from. */
	bool DiffersFromStartingValue(const IPropertyHandle& SettingHandle);

	/** Sets each live actor behind the handle to the value its setting starts from, as one undoable edit. */
	void ResetToStartingValues(IPropertyHandle& SettingHandle);
}

/** Live actor Details: the seed field with a new-seed button; each overridable setting's reset arrow going to its starting value; size and surfacer overrides inline, each value greyed while the build does not use it. */
class FCambrianaLiveActorCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
