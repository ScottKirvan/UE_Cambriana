#include "CambrianaGrammar.h"

UCambrianaGrammar::FOnGrammarChanged& UCambrianaGrammar::OnGrammarChanged()
{
	static FOnGrammarChanged Delegate;
	return Delegate;
}

#if WITH_EDITOR
void UCambrianaGrammar::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnGrammarChanged().Broadcast(this);
}

void UCambrianaGrammar::PostEditUndo()
{
	Super::PostEditUndo();
	OnGrammarChanged().Broadcast(this);
}
#endif
