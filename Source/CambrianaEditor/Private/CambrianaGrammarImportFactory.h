#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "CambrianaGrammarImportFactory.generated.h"

/** Imports .lsys files as they are and .ls files converted to .lsys; the extension alone sets the format. */
UCLASS()
class UCambrianaGrammarImportFactory : public UFactory
{
	GENERATED_BODY()

public:
	UCambrianaGrammarImportFactory();

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
		const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
