#pragma once

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/SoftObjectPtr.h"

#include "CambrianaAssetImportData.generated.h"

class UCambrianaGrammar;
class USkeletalMesh;
class UStaticMesh;
struct FAssetData;

/** A baked mesh's link to the grammar asset it was baked from; its source file is the grammar's package file. */
UCLASS()
class UCambrianaAssetImportData : public UAssetImportData
{
	GENERATED_BODY()

public:
	/** The grammar asset the mesh is rebuilt from. */
	UPROPERTY(VisibleAnywhere, Category = "Grammar")
	TSoftObjectPtr<UCambrianaGrammar> Grammar;

	/** Links to InGrammar and records its package file as the source file. */
	void SetGrammar(const UCambrianaGrammar& InGrammar);

	/** The linked grammar's path, following redirectors, when a grammar asset is there in memory or on disk; otherwise null. */
	FSoftObjectPath ResolveGrammarPath() const;

	/** The absolute package file of the asset at Path. */
	static FString PackageFilenameOf(const FSoftObjectPath& Path);

	/** The import data of a static or skeletal mesh, when it is a grammar link; otherwise null. */
	static UCambrianaAssetImportData* Of(const UObject* Mesh);

	/**
	 * Links the mesh to the grammar in its import data, replacing import data of another kind (as
	 * UFbxStaticMeshImportData::GetImportDataForStaticMesh does). Without a grammar, a link to one is replaced with plain import data
	 * with no source file, as the Source File row's Clear leaves it (AssetImportDataCustomization.cpp).
	 */
	static void Link(UStaticMesh& Mesh, const UCambrianaGrammar* Grammar);
	static void Link(USkeletalMesh& Mesh, const UCambrianaGrammar* Grammar);

	/**
	 * Whether the asset registry's record of the mesh says it may be linked to a grammar, without loading it: a static or skeletal
	 * mesh whose source file is an asset package file holding a grammar or a redirector to one, or, for a grammar moved since, whose
	 * package the registry says references a grammar's package. The record does not hold the import data's class, so a mesh this
	 * passes is confirmed by loading it (Of).
	 */
	static bool MayLinkToGrammar(const FAssetData& Mesh);

	/** The grammar's outputs: the static and skeletal meshes whose grammar link resolves to Grammar, found through the asset registry. */
	static TArray<FAssetData> FindMeshesLinkedTo(const UCambrianaGrammar& Grammar);
};
