#include "CambrianaGrammarWorkingCopy.h"

#include "CambrianaGrammar.h"
#include "Misc/ITransaction.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
	template <typename FunctionType>
	void ForEachGrammarProperty(FunctionType&& Function)
	{
		for (TFieldIterator<FProperty> It(UCambrianaGrammar::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
#if WITH_EDITORONLY_DATA
			if (It->GetFName() == GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, AssetImportData))
			{
				continue;
			}
#endif
			Function(**It);
		}
	}

	bool SameValue(const FProperty& Property, const UCambrianaGrammar& A, const UCambrianaGrammar& B)
	{
		// FString's operator==, and so FProperty::Identical, ignores case; grammar text and parameter names do not.
		if (Property.GetFName() == GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, ParameterValues))
		{
			return A.ParameterValues == B.ParameterValues;
		}
		if (const FStrProperty* String = CastField<FStrProperty>(&Property))
		{
			return String->GetPropertyValue_InContainer(&A).Equals(String->GetPropertyValue_InContainer(&B), ESearchCase::CaseSensitive);
		}
		return Property.Identical_InContainer(&A, &B);
	}
}

namespace Cambriana::Editor
{
	UCambrianaGrammar* DuplicateWorkingCopy(const UCambrianaGrammar& Grammar)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass(), *(Grammar.GetName() + TEXT("_WorkingCopy")));
		UCambrianaGrammar* WorkingCopy = CastChecked<UCambrianaGrammar>(
			StaticDuplicateObject(&Grammar, GetTransientPackage(), Name, ~(RF_Standalone | RF_Public)));
		WorkingCopy->SetFlags(RF_Transactional);
		return WorkingCopy;
	}

	bool HoldSameGrammar(const UCambrianaGrammar& A, const UCambrianaGrammar& B)
	{
		bool bSame = true;
		ForEachGrammarProperty([&](const FProperty& Property)
		{
			bSame = bSame && SameValue(Property, A, B);
		});
		return bSame;
	}

	bool CopyGrammar(const UCambrianaGrammar& From, UCambrianaGrammar& To)
	{
		if (HoldSameGrammar(From, To))
		{
			return false;
		}
		const TGuardValue<ITransaction*> OutsideUndoHistory(GUndo, nullptr);
		To.PreEditChange(nullptr);
		ForEachGrammarProperty([&](const FProperty& Property)
		{
			Property.CopyCompleteValue_InContainer(&To, &From);
		});
		FPropertyChangedEvent Event(nullptr);
		To.PostEditChangeProperty(Event);
		return true;
	}
}
