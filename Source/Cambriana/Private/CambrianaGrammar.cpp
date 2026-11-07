#include "CambrianaGrammar.h"

#include "EditorFramework/AssetImportData.h"

UCambrianaGrammar::FOnGrammarChanged& UCambrianaGrammar::OnGrammarChanged()
{
	static FOnGrammarChanged Delegate;
	return Delegate;
}

void UCambrianaGrammar::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

#if WITH_EDITORONLY_DATA
void UCambrianaGrammar::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	if (AssetImportData)
	{
		Context.AddTag(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
	Super::GetAssetRegistryTags(Context);
}
#endif

#if WITH_EDITOR
void UCambrianaGrammar::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnGrammarChanged().Broadcast(this);
}

void UCambrianaGrammar::PostEditUndo()
{
	Super::PostEditUndo();
	OnGrammarChanged().Broadcast(this);
}
#endif
