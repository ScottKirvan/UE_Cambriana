#include "CambrianaReimportFactory.h"

#include "Engine/StaticMesh.h"

UCambrianaReimportFactory::UCambrianaReimportFactory()
{
	SupportedClass = UStaticMesh::StaticClass();
	bCreateNew = false;
	bEditorImport = false;
}

bool UCambrianaReimportFactory::FactoryCanImport(const FString& Filename)
{
	return false;
}

bool UCambrianaReimportFactory::DoesSupportClass(UClass* Class)
{
	return false;
}

bool UCambrianaReimportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return false;
}

void UCambrianaReimportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
}

EReimportResult::Type UCambrianaReimportFactory::Reimport(UObject* Obj)
{
	return EReimportResult::Failed;
}

int32 UCambrianaReimportFactory::GetPriority() const
{
	return ImportPriority;
}

void UCambrianaReimportFactory::PostImportCleanUp()
{
}
