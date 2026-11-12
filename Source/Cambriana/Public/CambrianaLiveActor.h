#pragma once

#include "CoreMinimal.h"
#include "CambrianaBuild.h"
#include "DynamicMeshActor.h"

#include "CambrianaLiveActor.generated.h"

class UCambrianaGrammar;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** Dynamic-mesh actor that rebuilds its mesh from a grammar whenever its inputs change. */
UCLASS()
class CAMBRIANA_API ACambrianaLiveActor : public ADynamicMeshActor
{
	GENERATED_BODY()

public:
	ACambrianaLiveActor();

	UPROPERTY(EditAnywhere, Category = "Cambriana")
	TObjectPtr<UCambrianaGrammar> Grammar;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (MultiLine = "true", EditCondition = "Grammar == nullptr"))
	FString InlineGrammar;

	UPROPERTY(EditAnywhere, Category = "Cambriana")
	uint32 Seed = 0;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideRecursion = false;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideRecursion", ClampMin = "0"))
	double Recursion = 4.0;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideBasicAngle = false;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideBasicAngle", Units = "Degrees"))
	double BasicAngle = 25.0;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideThickness = false;

	/** Percent of segment length. */
	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideThickness", ClampMin = "0"))
	double Thickness = 50.0;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideMinThickness = false;

	/** The min thickness applies over .lsys text's minthickness and a grammar asset's min thickness. Without either it always applies. */
	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideMinThickness", ClampMin = "0", Units = "Centimeters"))
	double MinThickness = 0.0;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideClosedForm = false;

	/** The closed form applies over .lsys text's surface and a grammar asset's closed form. Without either it always applies. */
	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideClosedForm"))
	bool bClosedForm = true;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideSides = false;

	/** The sides apply over .lsys text's sides and a grammar asset's sides. Without either they always apply. */
	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideSides", ClampMin = "3"))
	int32 Sides = 8;

	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (InlineEditConditionToggle))
	bool bOverrideSize = false;

	/** The size mode and value apply over a grammar asset's size and over .lsys text's size or step. Without an asset, .ls text always uses them. */
	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideSize"))
	ECambrianaSizeMode SizeMode = ECambrianaSizeMode::Fit;

	/** Target size in Fit mode, step length in Step mode. */
	UPROPERTY(EditAnywhere, Category = "Cambriana", meta = (EditCondition = "bOverrideSize", HideEditConditionToggle, ClampMin = "0.01", Units = "Centimeters"))
	double SizeValue = 200.0;

	void SetGrammar(UCambrianaGrammar* NewGrammar);

	/** Placement from a grammar asset: references it and copies its seed. */
	void PlaceFromGrammar(UCambrianaGrammar* Source);

	FCambrianaBuildInput MakeBuildInput() const;

	void Rebuild();

	int32 GetBuildCount() const { return BuildCount; }
	const FCambrianaBuildInput& GetLastBuildInput() const { return LastBuildInput; }
	const FCambrianaBuildReport& GetLastBuildReport() const { return LastBuildReport; }

	static void RebuildActorsUsing(const UCambrianaGrammar* ChangedGrammar);

	virtual void PostActorCreated() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void RebuildIfInputChanged();
	void ShowSettingsUsed();
	void ApplyPreviewMaterials(TConstArrayView<int64> SlotMaterialIds);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PreviewBaseMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PreviewMaterials;

	int32 BuildCount = 0;
	FCambrianaBuildInput LastBuildInput;
	FCambrianaBuildReport LastBuildReport;
};
