#pragma once

#include "CoreMinimal.h"
#include "Exporters/Exporter.h"

#include "CambrianaGrammarExporter.generated.h"

/** Exports a grammar asset's text as a UTF-8 .lsys file; legacy .ls text is converted first. */
UCLASS()
class UCambrianaGrammarExporter : public UExporter
{
	GENERATED_BODY()

public:
	UCambrianaGrammarExporter();

	virtual bool ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags = 0) override;
};
