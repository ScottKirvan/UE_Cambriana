#include "CambrianaGrammar.h"

#include "EditorFramework/AssetImportData.h"

UCambrianaGrammar::FOnGrammarChanged& UCambrianaGrammar::OnGrammarChanged()
{
	static FOnGrammarChanged Delegate;
	return Delegate;
}

void UCambrianaGrammar::StartSettingsFromGrammarText()
{
	const UCambrianaGrammar* Default = GetDefault<UCambrianaGrammar>();
	const FCambrianaStatedSettings Stated = Cambriana::ReadStatedSettings(GrammarText);
	SizeMode = Stated.SizeMode.Get(Default->SizeMode);
	SizeValue = Stated.SizeValue.Get(Default->SizeValue);
	Sides = Stated.Sides.Get(Default->Sides);
	bClosedForm = Stated.bClosedForm.Get(Default->bClosedForm);
	MinThickness = Stated.MinThickness.Get(Default->MinThickness);
}

void UCambrianaGrammar::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	// As UTexture: a grammar being loaded takes its import data from its package; PostLoad fills the gap if the package has none.
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

void UCambrianaGrammar::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	if (!AssetImportData)
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
