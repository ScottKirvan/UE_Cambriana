#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FCambrianaGrammarPreview;

/** Grammar editor viewport showing the grammar's transient preview. */
class SCambrianaGrammarViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SCambrianaGrammarViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FCambrianaGrammarPreview>& InPreview);

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	TSharedPtr<FCambrianaGrammarPreview> Preview;
};
