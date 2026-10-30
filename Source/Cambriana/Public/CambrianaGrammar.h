#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "CambrianaGrammar.generated.h"

/** Interim grammar asset: legacy .ls text and a seed. */
UCLASS(BlueprintType)
class CAMBRIANA_API UCambrianaGrammar : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Grammar", meta = (MultiLine = "true"))
	FString GrammarText;

	UPROPERTY(EditAnywhere, Category = "Grammar")
	uint32 Seed = 0;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGrammarChanged, const UCambrianaGrammar*);

	/** Broadcast after any grammar asset changes. */
	static FOnGrammarChanged& OnGrammarChanged();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};
