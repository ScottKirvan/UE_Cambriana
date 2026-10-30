#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "CambrianaGrammarFactory.generated.h"

UCLASS()
class UCambrianaGrammarFactory : public UFactory
{
	GENERATED_BODY()

public:
	UCambrianaGrammarFactory();

	static const TCHAR* DefaultGrammarText;

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
