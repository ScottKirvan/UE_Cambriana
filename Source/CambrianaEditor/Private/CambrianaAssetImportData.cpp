#include "CambrianaAssetImportData.h"

#include "CambrianaGrammar.h"

void UCambrianaAssetImportData::SetGrammar(const UCambrianaGrammar& InGrammar)
{
}

FSoftObjectPath UCambrianaAssetImportData::ResolveGrammarPath() const
{
	return FSoftObjectPath();
}

FString UCambrianaAssetImportData::PackageFilenameOf(const FSoftObjectPath& Path)
{
	return FString();
}

UCambrianaAssetImportData* UCambrianaAssetImportData::Of(const UObject* Mesh)
{
	return nullptr;
}
