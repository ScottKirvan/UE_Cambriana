#pragma once

#include "CoreMinimal.h"
#include "CambrianaBuild.h"
#include "UObject/Object.h"

#include "CambrianaGrammar.generated.h"

class UAssetImportData;

/** What Bake writes besides the skeletal mesh, skeleton and physics asset, and how stiff the physics asset is. */
USTRUCT()
struct FCambrianaBakeSettings
{
	GENERATED_BODY()

	/** Also write a static mesh, SM_<grammar asset name>. */
	UPROPERTY(EditAnywhere, Category = "Bake")
	bool bStaticMesh = false;

	/**
	 * Scales the whole plant's stiffness: each physics body's drive back to its bind pose has this times radius^4 / length of the
	 * body's bones, in centimetres, as its stiffness.
	 */
	UPROPERTY(EditAnywhere, Category = "Bake", meta = (ClampMin = "0"))
	float PhysicsStiffness = 25000.0f;

	bool operator==(const FCambrianaBakeSettings&) const = default;
};

/** Interim grammar asset: grammar text (.lsys, or legacy .ls), a seed, parameter values, size, surfacer and bake settings and, when imported, its source file. */
UCLASS(BlueprintType)
class CAMBRIANA_API UCambrianaGrammar : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Grammar", meta = (MultiLine = "true"))
	FString GrammarText;

	UPROPERTY(EditAnywhere, Category = "Grammar")
	uint32 Seed = 0;

	/** Values set for the text's parameters; a parameter without one uses the text's value. Kept when the text stops declaring the name. */
	UPROPERTY()
	TArray<FCambrianaParameterValue> ParameterValues;

	/** Used by live actors without a size override; for .lsys text, only when the text states neither size nor step. */
	UPROPERTY(EditAnywhere, Category = "Size")
	ECambrianaSizeMode SizeMode = ECambrianaSizeMode::Fit;

	/** Target size in Fit mode, step length in Step mode. */
	UPROPERTY(EditAnywhere, Category = "Size", meta = (ClampMin = "0.01", Units = "Centimeters"))
	double SizeValue = 200.0;

	UPROPERTY(EditAnywhere, Category = "Surfacer")
	bool bClosedForm = true;

	UPROPERTY(EditAnywhere, Category = "Surfacer", meta = (ClampMin = "3"))
	int32 Sides = 8;

	UPROPERTY(EditAnywhere, Category = "Surfacer", meta = (ClampMin = "0", Units = "Centimeters"))
	double MinThickness = 0.0;

	UPROPERTY(EditAnywhere, Category = "Bake", meta = (ShowOnlyInnerProperties))
	FCambrianaBakeSettings Bake;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

	/** Sets each size and surfacer setting to the grammar text's, or the default where the text states none. */
	void StartSettingsFromGrammarText();

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGrammarChanged, const UCambrianaGrammar*);

	/** Broadcast after any grammar asset changes. */
	static FOnGrammarChanged& OnGrammarChanged();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};
