#pragma once

#include "CoreMinimal.h"
#include "CambrianaBuild.h"
#include "UObject/GCObject.h"

class FAdvancedPreviewScene;
class UCambrianaGrammar;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWorld;

/** Transient preview of a grammar asset: what a live actor referencing it builds with every override off and the asset's seed. */
class FCambrianaGrammarPreview : public FGCObject
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnRebuilt);

	FCambrianaGrammarPreview();
	virtual ~FCambrianaGrammarPreview() override;

	FCambrianaGrammarPreview(const FCambrianaGrammarPreview&) = delete;
	FCambrianaGrammarPreview& operator=(const FCambrianaGrammarPreview&) = delete;

	/** Rebuilds unless the grammar's build input equals the last build's. */
	void Update(const UCambrianaGrammar& Grammar);

	FAdvancedPreviewScene& GetScene() const { return *Scene; }
	UWorld* GetWorld() const;
	const UDynamicMeshComponent* GetMeshComponent() const { return MeshComponent; }
	int32 GetBuildCount() const { return BuildCount; }
	const FCambrianaBuildInput& GetLastBuildInput() const { return LastBuildInput; }
	const FCambrianaBuildReport& GetLastBuildReport() const { return LastBuildReport; }
	FOnRebuilt& OnRebuilt() { return RebuiltDelegate; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	TSharedRef<FAdvancedPreviewScene> Scene;
	TObjectPtr<UDynamicMeshComponent> MeshComponent;
	TObjectPtr<UMaterialInterface> BaseMaterial;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Palette;
	int32 BuildCount = 0;
	FCambrianaBuildInput LastBuildInput;
	FCambrianaBuildReport LastBuildReport;
	FOnRebuilt RebuiltDelegate;
};
