#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"

#include "CambrianaGrammarImportFactory.generated.h"

/** Imports and reimports .lsys files as they are and .ls files converted to .lsys; the extension alone sets the format. */
UCLASS()
class UCambrianaGrammarImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UCambrianaGrammarImportFactory();

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
		const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
};
