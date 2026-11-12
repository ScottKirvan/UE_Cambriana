#pragma once

#include "CoreMinimal.h"
#include "CambrianaBuild.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UCambrianaGrammar;

namespace Cambriana::Editor
{
	/** Stores Value for the grammar's parameter Name as one undoable change; nothing happens if it is already stored. */
	void SetParameterValue(UCambrianaGrammar& Grammar, const FString& Name, double Value);

	/** Removes the grammar's stored value for Name as one undoable change, so the parameter uses the text's value again. */
	void ResetParameterValue(UCambrianaGrammar& Grammar, const FString& Name);
}

/** Grammar asset Details: the seed row, and the text's parameters in one category per tier. */
class FCambrianaGrammarCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual ~FCambrianaGrammarCustomization() override;

	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	void AddParameterRows(IDetailLayoutBuilder& DetailBuilder);
	void HandleGrammarChanged(const UCambrianaGrammar* ChangedGrammar);
	const FCambrianaParameter* FindParameter(const FString& Name) const;

	TWeakObjectPtr<UCambrianaGrammar> Grammar;
	TWeakPtr<IDetailLayoutBuilder> LayoutBuilder;
	TArray<FCambrianaParameter> Parameters;
	FDelegateHandle GrammarChangedHandle;
};
