#pragma once

#include "AssetSelection.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarFormat.h"
#include "CambrianaLiveActor.h"
#include "CoreMinimal.h"
#include "Editor.h"
#include "Elements/Actor/ActorElementData.h"
#include "IDetailTreeNode.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "PropertyHandle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SWidget.h"

#include <cambriana/cambriana.h>

namespace Cambriana::Editor::Tests
{
	inline UCambrianaGrammar* CreateGrammarWithFactory()
	{
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		return Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass()), RF_Transactional, nullptr, nullptr));
	}

	inline FCambrianaGrammarEditorToolkit* OpenGrammarEditor(UCambrianaGrammar* Grammar)
	{
		UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (!AssetEditors->OpenEditorForAsset(Grammar))
		{
			return nullptr;
		}
		IAssetEditorInstance* Editor = AssetEditors->FindEditorForAsset(Grammar, false);
		return Editor && Editor->GetEditorName() == FCambrianaGrammarEditorToolkit::ToolkitName
			? static_cast<FCambrianaGrammarEditorToolkit*>(Editor)
			: nullptr;
	}

	inline void CloseGrammarEditor(UCambrianaGrammar* Grammar)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Grammar);
	}

	inline TSharedPtr<SWidget> FindWidgetOfType(const TSharedRef<SWidget>& Root, FName Type)
	{
		if (Root->GetType() == Type)
		{
			return Root;
		}
		FChildren* Children = Root->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SWidget> Found = FindWidgetOfType(Children->GetChildAt(Index), Type))
			{
				return Found;
			}
		}
		return nullptr;
	}

	inline TSharedPtr<IPropertyHandle> FindPropertyHandle(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, FName PropertyName)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() && Handle->GetProperty()->GetFName() == PropertyName)
			{
				return Handle;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			if (TSharedPtr<IPropertyHandle> Found = FindPropertyHandle(Children, PropertyName))
			{
				return Found;
			}
		}
		return nullptr;
	}

	// Each tube ring has one vertex per side, and an unbranched chain of N segments has N + 1 rings (SDD-surfacing.md, Tubes).
	constexpr int32 TubeVertices(int32 Segments, int32 Sides)
	{
		return (Segments + 1) * Sides;
	}

	inline ACambrianaLiveActor* PlaceInCurrentLevel(UCambrianaGrammar* Grammar)
	{
		UE::AssetPlacementUtil::FExtraPlaceAssetOptions Options;
		Options.bSelectOutput = false;
		const TArray<FTypedElementHandle> Placed = UE::AssetPlacementUtil::PlaceAssetInCurrentLevel(Grammar, Options);
		return Placed.Num() == 1 ? Cast<ACambrianaLiveActor>(ActorElementDataUtil::GetActorFromHandle(Placed[0])) : nullptr;
	}

	inline FString FixturePath(const TCHAR* FileName)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Cambriana"));
		return Plugin
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/Cambriana/Private/Tests/Fixtures"), FileName))
			: FString();
	}

	class FCoreResult
	{
	public:
		FCoreResult(ECambrianaGrammarFormat Format, const FString& Text, uint32 Seed)
		{
			cambriana_build_settings Settings;
			cambriana_build_settings_init(&Settings, sizeof(Settings));
			Settings.seed = Seed;
			Settings.overrides |= CAMBRIANA_OVERRIDE_SEED;
			const FTCHARToUTF8 Utf8(*Text);
			if (Format == ECambrianaGrammarFormat::Legacy)
			{
				cambriana_build_legacy(Utf8.Get(), Utf8.Length(), &Settings, nullptr, &Result);
			}
			else
			{
				cambriana_build_grammar(Utf8.Get(), Utf8.Length(), nullptr, &Settings, nullptr, &Result);
			}
		}

		~FCoreResult() { cambriana_result_destroy(Result); }

		FCoreResult(const FCoreResult&) = delete;
		FCoreResult& operator=(const FCoreResult&) = delete;

		bool IsValid() const { return Result != nullptr; }

		bool HasErrors() const { return (cambriana_result_summary(Result)->flags & CAMBRIANA_RESULT_HAS_ERRORS) != 0; }

		TConstArrayView<cambriana_bone> Bones() const
		{
			uint64_t Count = 0;
			const cambriana_bone* Data = cambriana_result_bones(Result, &Count);
			return TConstArrayView<cambriana_bone>(Data, static_cast<int32>(Count));
		}

	private:
		cambriana_result* Result = nullptr;
	};

	inline bool SameVector(const cambriana_vec3& A, const cambriana_vec3& B)
	{
		return A.x == B.x && A.y == B.y && A.z == B.z;
	}

	inline bool SameBone(const cambriana_bone& A, const cambriana_bone& B)
	{
		return SameVector(A.start, B.start) && SameVector(A.end, B.end) && SameVector(A.frame.side, B.frame.side)
			&& SameVector(A.frame.forward, B.frame.forward) && SameVector(A.frame.up, B.frame.up) && A.radius == B.radius
			&& A.parent == B.parent && A.colour == B.colour && A.generation == B.generation && A.kind == B.kind
			&& A.symbol == B.symbol && A.chain_break_before == B.chain_break_before
			&& FCStringAnsi::Strcmp(A.name ? A.name : "", B.name ? B.name : "") == 0;
	}

	inline int32 CountBoneMismatches(TConstArrayView<cambriana_bone> A, TConstArrayView<cambriana_bone> B)
	{
		int32 Mismatches = 0;
		for (int32 Index = 0; Index < FMath::Min(A.Num(), B.Num()); ++Index)
		{
			Mismatches += !SameBone(A[Index], B[Index]);
		}
		return Mismatches;
	}
}
