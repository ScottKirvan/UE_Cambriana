#pragma once

#include "CambrianaGrammar.h"
#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
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

	// Each tube ring has one vertex per side, and an unbranched chain of N segments has N + 1 rings (SDD-surfacing.md, Tubes).
	constexpr int32 TubeVertices(int32 Segments, int32 Sides)
	{
		return (Segments + 1) * Sides;
	}

	/** Each bone's reference pose in component space. */
	inline TArray<FTransform> ComponentSpacePose(const FReferenceSkeleton& Skeleton)
	{
		const TArray<FTransform>& Local = Skeleton.GetRefBonePose();
		TArray<FTransform> Component;
		Component.SetNum(Local.Num());
		for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
		{
			const int32 Parent = Skeleton.GetParentIndex(Bone);
			Component[Bone] = Parent == INDEX_NONE ? Local[Bone] : Local[Bone] * Component[Parent];
		}
		return Component;
	}

	inline UCambrianaGrammar* MakeGrammar(const FString& Text)
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
		Grammar->GrammarText = Text;
		return Grammar;
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
