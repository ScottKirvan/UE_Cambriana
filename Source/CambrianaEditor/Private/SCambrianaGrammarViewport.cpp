#include "SCambrianaGrammarViewport.h"

#include "AdvancedPreviewScene.h"
#include "CambrianaGrammarPreview.h"
#include "Components/DynamicMeshComponent.h"
#include "EditorViewportClient.h"

namespace
{
	class FCambrianaGrammarViewportClient : public FEditorViewportClient
	{
	public:
		FCambrianaGrammarViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewport)
			: FEditorViewportClient(nullptr, &InPreviewScene, InViewport)
		{
			SetViewMode(VMI_Lit);
			bSetListenerPosition = false;
		}

		virtual bool ShouldOrbitCamera() const override { return true; }
	};
}

void SCambrianaGrammarViewport::Construct(const FArguments& InArgs, const TSharedRef<FCambrianaGrammarPreview>& InPreview)
{
	Preview = InPreview;
	SEditorViewport::Construct(SEditorViewport::FArguments());
	PreviewRebuiltHandle = Preview->OnRebuilt().AddSP(this, &SCambrianaGrammarViewport::HandlePreviewRebuilt);
	FrameMesh(true);
}

SCambrianaGrammarViewport::~SCambrianaGrammarViewport()
{
	if (Preview)
	{
		Preview->OnRebuilt().Remove(PreviewRebuiltHandle);
	}
}

TSharedRef<FEditorViewportClient> SCambrianaGrammarViewport::MakeEditorViewportClient()
{
	const TSharedRef<FCambrianaGrammarViewportClient> ViewportClient = MakeShared<FCambrianaGrammarViewportClient>(Preview->GetScene(), SharedThis(this));
	ViewportClient->SetViewRotation(FRotator(-20.0, -45.0, 0.0));
	return ViewportClient;
}

void SCambrianaGrammarViewport::OnFocusViewportToSelection()
{
	FrameMesh(false);
}

void SCambrianaGrammarViewport::FrameMesh(bool bInstant)
{
	Client->FocusViewportOnBox(Preview->GetMeshComponent()->Bounds.GetBox(), bInstant);
}

void SCambrianaGrammarViewport::HandlePreviewRebuilt()
{
	// The viewport is not realtime, so without this it draws the new mesh only on the next input.
	Client->Invalidate();
}
