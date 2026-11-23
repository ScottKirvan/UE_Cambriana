#include "CambrianaAssetImportData.h"

#include "AssetRegistry/AssetData.h"
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

namespace
{
	template <typename MeshType>
	void LinkMesh(MeshType& Mesh, const UCambrianaGrammar* Grammar)
	{
		UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(&Mesh);
		if (!Grammar)
		{
			if (ImportData)
			{
				Mesh.SetAssetImportData(NewObject<UAssetImportData>(&Mesh, NAME_None, RF_NoFlags));
			}
			return;
		}
		if (!ImportData)
		{
			ImportData = NewObject<UCambrianaAssetImportData>(&Mesh, NAME_None, RF_NoFlags);
			Mesh.SetAssetImportData(ImportData);
		}
		ImportData->SetGrammar(*Grammar);
	}
}

void UCambrianaAssetImportData::Link(UStaticMesh& Mesh, const UCambrianaGrammar* Grammar)
{
	LinkMesh(Mesh, Grammar);
}

void UCambrianaAssetImportData::Link(USkeletalMesh& Mesh, const UCambrianaGrammar* Grammar)
{
	LinkMesh(Mesh, Grammar);
}

TArray<FAssetData> UCambrianaAssetImportData::FindMeshesLinkedTo(const UCambrianaGrammar& Grammar)
{
	return {};
}
