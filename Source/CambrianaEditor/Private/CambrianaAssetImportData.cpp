#include "CambrianaAssetImportData.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AutoReimport/AssetSourceFilenameCache.h"
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

namespace
{
	/** Whether the registry says the package holds a grammar asset, or a redirector to one. */
	bool PackageHoldsGrammar(const IAssetRegistry& Registry, FName PackageName)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(PackageName, Assets);
		return Assets.ContainsByPredicate([&Registry](const FAssetData& Asset)
		{
			const FAssetData Target = Asset.IsRedirector() ? Registry.GetAssetByObjectPath(Registry.GetRedirectedObjectPath(Asset.GetSoftObjectPath())) : Asset;
			return Target.IsInstanceOf(UCambrianaGrammar::StaticClass());
		});
	}
}

bool UCambrianaAssetImportData::MayLinkToGrammar(const FAssetData& Mesh)
{
	if (!Mesh.IsInstanceOf(UStaticMesh::StaticClass()) && !Mesh.IsInstanceOf(USkeletalMesh::StaticClass()))
	{
		return false;
	}
	const TOptional<FAssetImportInfo> ImportInfo = FAssetSourceFilenameCache::ExtractAssetImportInfo(Mesh);
	if (!ImportInfo)
	{
		return false;
	}
	const IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	const FString MeshFolder = FPackageName::LongPackageNameToFilename(Mesh.PackagePath.ToString() / TEXT(""));
	for (const FAssetImportInfo::FSourceFile& File : ImportInfo->SourceFiles)
	{
		if (FPaths::GetExtension(File.RelativeFilename, true) != FPackageName::GetAssetPackageExtension())
		{
			continue;
		}
		// Resolved as FAssetSourceFilenameCache::GetAssetsPertainingToFile resolves a source file: relative to the asset's folder, or absolute.
		const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::IsRelative(File.RelativeFilename) ? MeshFolder / File.RelativeFilename : File.RelativeFilename);
		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName) && PackageHoldsGrammar(Registry, FName(PackageName)))
		{
			return true;
		}
		// A grammar moved without leaving a redirector: the source file names its old package until the next Bake, but once the
		// mesh is saved the registry records its reference to the grammar's new package.
		TArray<FName> Dependencies;
		Registry.GetDependencies(Mesh.PackageName, Dependencies);
		if (Dependencies.ContainsByPredicate([&Registry](FName Dependency) { return PackageHoldsGrammar(Registry, Dependency); }))
		{
			return true;
		}
	}
	return false;
}

TArray<FAssetData> UCambrianaAssetImportData::FindMeshesLinkedTo(const UCambrianaGrammar& Grammar)
{
	const FSoftObjectPath GrammarPath(&Grammar);
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();

	// Two registry lookups: the assets whose source file is the grammar's package file, as auto reimport finds a changed file's
	// assets (AssetSourceFilenameCache.cpp, which reads the registry's in-memory tags, so it follows unsaved links); and the packages
	// that reference the grammar's, as the Reference Viewer does, which follows a moved grammar once its referencers are saved.
	TArray<FAssetData> Candidates = FAssetSourceFilenameCache::Get().GetAssetsPertainingToFile(Registry, PackageFilenameOf(GrammarPath));
	TArray<FName> Referencers;
	Registry.GetReferencers(GrammarPath.GetLongPackageFName(), Referencers);
	for (const FName Referencer : Referencers)
	{
		Registry.GetAssetsByPackageName(Referencer, Candidates);
	}

	TArray<FAssetData> Linked;
	for (const FAssetData& Candidate : Candidates)
	{
		if (!MayLinkToGrammar(Candidate) || Linked.ContainsByPredicate([&Candidate](const FAssetData& Found) { return Found.GetSoftObjectPath() == Candidate.GetSoftObjectPath(); }))
		{
			continue;
		}
		const UCambrianaAssetImportData* ImportData = Of(Candidate.GetAsset());
		if (ImportData && ImportData->ResolveGrammarPath() == GrammarPath)
		{
			Linked.Add(Candidate);
		}
	}
	return Linked;
}
