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
	virtual ~SCambrianaGrammarViewport() override;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void OnFocusViewportToSelection() override;

private:
	void FrameMesh(bool bInstant);
	void HandlePreviewRebuilt();

	TSharedPtr<FCambrianaGrammarPreview> Preview;
	FDelegateHandle PreviewRebuiltHandle;
};
