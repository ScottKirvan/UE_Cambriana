#include "CambrianaReimportFactory.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "CambrianaAssetImportData.h"
#include "CambrianaBake.h"
#include "CambrianaGrammar.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogCambrianaReimport, Log, All);

namespace
{
	/** The grammar asset whose package file Filename is, or null. */
	UCambrianaGrammar* GrammarInPackageFile(const FString& Filename)
	{
		FString PackageName;
		if (!FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName))
		{
			return nullptr;
		}
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssetsByPackageName(*PackageName, Assets);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf(UCambrianaGrammar::StaticClass()))
			{
				return Cast<UCambrianaGrammar>(Asset.GetAsset());
			}
		}
		return nullptr;
	}
}

// As the engine's FBX reimport factories (ReimportFbxStaticMeshFactory.h): a reimport-only factory that imports no file.
UCambrianaReimportFactory::UCambrianaReimportFactory()
{
	SupportedClass = UStaticMesh::StaticClass();
	bCreateNew = false;
	// Kept out of the Import dialog's file types; Reimport With New File still offers the source file's .uasset type (FReimportManager::GetNewReimportPath).
	bEditorImport = false;
	// Above Interchange's handler, DefaultImportPriority + 10 (InterchangeReimportHandler.cpp), which is above the legacy FBX handlers.
	ImportPriority = DefaultImportPriority + 20;
}

bool UCambrianaReimportFactory::FactoryCanImport(const FString& Filename)
{
	return false;
}

bool UCambrianaReimportFactory::DoesSupportClass(UClass* Class)
{
	return Class && (Class->IsChildOf<UStaticMesh>() || Class->IsChildOf<USkeletalMesh>());
}

bool UCambrianaReimportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Obj);
	const FSoftObjectPath Grammar = ImportData ? ImportData->ResolveGrammarPath() : FSoftObjectPath();
	if (Grammar.IsNull())
	{
		return false;
	}
	OutFilenames.Add(UCambrianaAssetImportData::PackageFilenameOf(Grammar));
	return true;
}

void UCambrianaReimportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Obj);
	if (!ImportData || NewReimportPaths.Num() != 1)
	{
		return;
	}
	RejectedNewFiles.Remove(FObjectKey(Obj));
	if (const UCambrianaGrammar* Grammar = GrammarInPackageFile(NewReimportPaths[0]))
	{
		Obj->Modify();
		ImportData->SetGrammar(*Grammar);
		return;
	}
	UE_LOG(LogCambrianaReimport, Error, TEXT("%s: %s is not a grammar asset's package file; the mesh stays linked to %s"), *Obj->GetPathName(),
		*NewReimportPaths[0], *ImportData->Grammar.ToString());
	RejectedNewFiles.Add(FObjectKey(Obj));
}

EReimportResult::Type UCambrianaReimportFactory::Reimport(UObject* Obj)
{
	if (RejectedNewFiles.Remove(FObjectKey(Obj)) > 0)
	{
		return EReimportResult::Failed;
	}
	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Obj);
	UCambrianaGrammar* Grammar = ImportData ? Cast<UCambrianaGrammar>(ImportData->ResolveGrammarPath().TryLoad()) : nullptr;
	if (!Grammar)
	{
		UE_LOG(LogCambrianaReimport, Error, TEXT("%s: its grammar is not found; nothing is rebuilt"), *GetPathNameSafe(Obj));
		return EReimportResult::Failed;
	}
	return Cambriana::Editor::RebakeMesh(*Grammar, *Obj).Status == ECambrianaBakeStatus::Baked ? EReimportResult::Succeeded : EReimportResult::Failed;
}

int32 UCambrianaReimportFactory::GetPriority() const
{
	return ImportPriority;
}

void UCambrianaReimportFactory::PostImportCleanUp()
{
	RejectedNewFiles.Reset();
}
