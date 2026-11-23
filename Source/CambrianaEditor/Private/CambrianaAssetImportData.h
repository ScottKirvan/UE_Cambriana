#pragma once

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/SoftObjectPtr.h"

#include "CambrianaAssetImportData.generated.h"

class UCambrianaGrammar;

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
};
