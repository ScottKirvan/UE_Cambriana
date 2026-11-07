#include "AssetDefinition_CambrianaGrammar.h"

#include "CambrianaEditorCategory.h"
#include "CambrianaGrammar.h"

#define LOCTEXT_NAMESPACE "CambrianaEditor"

FText UAssetDefinition_CambrianaGrammar::GetAssetDisplayName() const
{
	return LOCTEXT("CambrianaGrammarDisplayName", "Cambriana Grammar");
}

FLinearColor UAssetDefinition_CambrianaGrammar::GetAssetColor() const
{
	return FLinearColor(FColor(64, 160, 96));
}

TSoftClassPtr<UObject> UAssetDefinition_CambrianaGrammar::GetAssetClass() const
{
	return UCambrianaGrammar::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_CambrianaGrammar::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(Cambriana::Editor::BojuStudiosCategoryText()) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
