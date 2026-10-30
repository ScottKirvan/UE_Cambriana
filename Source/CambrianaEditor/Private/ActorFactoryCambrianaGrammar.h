#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"

#include "ActorFactoryCambrianaGrammar.generated.h"

/** Places a live actor referencing a dropped grammar asset. */
UCLASS()
class UActorFactoryCambrianaGrammar : public UActorFactory
{
	GENERATED_BODY()

public:
	UActorFactoryCambrianaGrammar();

	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
};
