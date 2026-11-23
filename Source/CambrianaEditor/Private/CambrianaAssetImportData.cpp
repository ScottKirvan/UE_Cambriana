#include "CambrianaAssetImportData.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "CambrianaGrammar.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

void UCambrianaAssetImportData::SetGrammar(const UCambrianaGrammar& InGrammar)
{
	const FSoftObjectPath Path(&InGrammar);
	Grammar = TSoftObjectPtr<UCambrianaGrammar>(Path);
	Update(PackageFilenameOf(Path));
}

FSoftObjectPath UCambrianaAssetImportData::ResolveGrammarPath() const
{
	if (const UCambrianaGrammar* Loaded = Grammar.Get())
	{
		return FSoftObjectPath(Loaded);
	}
	if (Grammar.IsNull())
	{
		return FSoftObjectPath();
	}
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	const FSoftObjectPath Path = Registry.GetRedirectedObjectPath(Grammar.ToSoftObjectPath());
	const FAssetData Asset = Registry.GetAssetByObjectPath(Path);
	return Asset.IsValid() && Asset.IsInstanceOf(UCambrianaGrammar::StaticClass()) ? Path : FSoftObjectPath();
}

FString UCambrianaAssetImportData::PackageFilenameOf(const FSoftObjectPath& Path)
{
	return FPaths::ConvertRelativePathToFull(
		FPackageName::LongPackageNameToFilename(Path.GetLongPackageName(), FPackageName::GetAssetPackageExtension()));
}

UCambrianaAssetImportData* UCambrianaAssetImportData::Of(const UObject* Mesh)
{
	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
	{
		return Cast<UCambrianaAssetImportData>(StaticMesh->GetAssetImportData());
	}
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
	{
		return Cast<UCambrianaAssetImportData>(SkeletalMesh->GetAssetImportData());
	}
	return nullptr;
}
