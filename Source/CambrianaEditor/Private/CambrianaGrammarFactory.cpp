#include "CambrianaGrammarFactory.h"

#include "CambrianaGrammar.h"
#include "CambrianaSeed.h"

const TCHAR* UCambrianaGrammarFactory::DefaultGrammarText = TEXT(
	"generations 4\n"
	"angle       25\n"
	"thickness   50%\n"
	"\n"
	"axiom A\n"
	"\n"
	"grow {\n"
	"    A -> F ^ ' A\n"
	"}\n");

UCambrianaGrammarFactory::UCambrianaGrammarFactory()
{
	SupportedClass = UCambrianaGrammar::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UCambrianaGrammarFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(InParent, InClass, InName, Flags);
	Grammar->GrammarText = DefaultGrammarText;
	Grammar->Seed = Cambriana::GenerateSeed();
	return Grammar;
}
