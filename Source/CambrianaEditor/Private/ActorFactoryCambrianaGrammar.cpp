#include "ActorFactoryCambrianaGrammar.h"

#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"

#define LOCTEXT_NAMESPACE "CambrianaEditor"

UActorFactoryCambrianaGrammar::UActorFactoryCambrianaGrammar()
{
	DisplayName = LOCTEXT("LiveActorFactoryDisplayName", "Cambriana Live Actor");
	NewActorClass = ACambrianaLiveActor::StaticClass();
}

bool UActorFactoryCambrianaGrammar::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.IsInstanceOf(UCambrianaGrammar::StaticClass()))
	{
		OutErrorMsg = LOCTEXT("NoGrammar", "A valid Cambriana grammar must be specified.");
		return false;
	}
	return true;
}

void UActorFactoryCambrianaGrammar::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);
	CastChecked<ACambrianaLiveActor>(NewActor)->SetGrammar(Cast<UCambrianaGrammar>(Asset));
}

UObject* UActorFactoryCambrianaGrammar::GetAssetFromActorInstance(AActor* ActorInstance)
{
	return CastChecked<ACambrianaLiveActor>(ActorInstance)->Grammar;
}

#undef LOCTEXT_NAMESPACE
