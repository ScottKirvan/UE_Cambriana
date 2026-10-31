#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"

namespace Cambriana::Tests
{
	inline FString LoadFixture(const TCHAR* FileName)
	{
		FString Text;
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Cambriana")))
		{
			FFileHelper::LoadFileToString(Text,
				*FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/Cambriana/Private/Tests/Fixtures"), FileName));
		}
		return Text;
	}

	inline FString SimpleGrammar(int32 Recursion)
	{
		return FString::Printf(TEXT("%d\n25\n50\nA\nA=F^'A\n@\n"), Recursion);
	}

#if WITH_EDITOR
	inline void EditProperty(UObject* Object, FName PropertyName, TFunctionRef<void()> Change)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		Object->PreEditChange(Property);
		Change();
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Object->PostEditChangeProperty(Event);
	}
#endif
}
