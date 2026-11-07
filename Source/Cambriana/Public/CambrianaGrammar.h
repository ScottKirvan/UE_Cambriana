#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "CambrianaGrammar.generated.h"

class UAssetImportData;

/** Interim grammar asset: grammar text (.lsys, or legacy .ls), a seed and, when imported, its source file. */
UCLASS(BlueprintType)
class CAMBRIANA_API UCambrianaGrammar : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Grammar", meta = (MultiLine = "true"))
	FString GrammarText;

	UPROPERTY(EditAnywhere, Category = "Grammar")
	uint32 Seed = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGrammarChanged, const UCambrianaGrammar*);

	/** Broadcast after any grammar asset changes. */
	static FOnGrammarChanged& OnGrammarChanged();

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};
