#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "UObject/ObjectKey.h"

#include "CambrianaReimportFactory.generated.h"

/**
 * Reimports a baked static or skeletal mesh from the grammar asset its UCambrianaAssetImportData links, through the grammar's Bake.
 * Reimport With New File takes a grammar asset's package file and links the mesh to that grammar.
 */
UCLASS()
class UCambrianaReimportFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UCambrianaReimportFactory();

	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual bool DoesSupportClass(UClass* Class) override;

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	virtual void PostImportCleanUp() override;

private:
	/** Meshes given a new file that is not a grammar's package file, whose next Reimport fails. */
	TSet<FObjectKey> RejectedNewFiles;
};
