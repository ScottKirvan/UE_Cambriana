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
}

TSharedRef<FEditorViewportClient> SCambrianaGrammarViewport::MakeEditorViewportClient()
{
	const TSharedRef<FCambrianaGrammarViewportClient> ViewportClient = MakeShared<FCambrianaGrammarViewportClient>(Preview->GetScene(), SharedThis(this));
	const FBoxSphereBounds Bounds = Preview->GetMeshComponent()->Bounds;
	const bool bHasBounds = Bounds.SphereRadius > UE_KINDA_SMALL_NUMBER;
	ViewportClient->SetViewRotation(FRotator(-20.0, -45.0, 0.0));
	ViewportClient->SetViewLocationForOrbiting(bHasBounds ? Bounds.Origin : FVector::ZeroVector, static_cast<float>(bHasBounds ? Bounds.SphereRadius * 3.0 : 500.0));
	return ViewportClient;
}
