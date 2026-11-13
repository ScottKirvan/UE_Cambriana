#pragma once

#include "AssetSelection.h"
#include "CambrianaGrammar.h"
#include "CambrianaGrammarEditorToolkit.h"
#include "CambrianaGrammarFactory.h"
#include "CambrianaGrammarFormat.h"
#include "CambrianaGrammarPreview.h"
#include "CambrianaLiveActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Elements/Actor/ActorElementData.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
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

	inline FCambrianaGrammarEditorToolkit* FindGrammarEditor(UCambrianaGrammar* Grammar)
	{
		IAssetEditorInstance* Editor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Grammar, false);
		return Editor && Editor->GetEditorName() == FCambrianaGrammarEditorToolkit::ToolkitName
			? static_cast<FCambrianaGrammarEditorToolkit*>(Editor)
			: nullptr;
	}

	inline FCambrianaGrammarEditorToolkit* OpenGrammarEditor(UCambrianaGrammar* Grammar)
	{
		return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Grammar) ? FindGrammarEditor(Grammar) : nullptr;
	}

	/** Closes the grammar's editor, answering its close prompt with No so unsaved edits are discarded; left to the unattended dialog, the prompt would answer Cancel and keep the editor open. */
	inline void CloseGrammarEditor(UCambrianaGrammar* Grammar)
	{
		if (FCambrianaGrammarEditorToolkit* Toolkit = FindGrammarEditor(Grammar))
		{
			Toolkit->SetUnsavedEditsPrompt([](EAppMsgType::Type, const FText&) { return EAppReturnType::No; });
		}
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Grammar);
	}

	/** Closes the grammar editor as its tab's close button does, the one close the editor can stop; returns whether it closed. */
	inline bool CloseFromTab(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		const TSharedPtr<SDockTab> Tab = Toolkit.GetTabManager() ? Toolkit.GetTabManager()->GetOwnerTab() : nullptr;
		return Tab && Tab->RequestCloseTab();
	}

	/** Closes the grammar's editors as the asset editor subsystem does, a close the editor cannot stop, leaving any close prompt to whatever answer the editor has been given. */
	inline void CloseAsking(UCambrianaGrammar* Grammar)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Grammar);
	}

	inline const FCambrianaParameterValue* FindStoredValue(const UCambrianaGrammar& Grammar, const TCHAR* Name)
	{
		return Grammar.ParameterValues.FindByPredicate([Name](const FCambrianaParameterValue& Value) { return Value.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	/** The preview mesh's vertex count; INDEX_NONE without a preview mesh. */
	inline int32 PreviewVertexCount(const FCambrianaGrammarEditorToolkit& Toolkit)
	{
		const UDynamicMeshComponent* Mesh = Toolkit.GetPreview().GetMeshComponent();
		return Mesh ? Mesh->GetMesh()->VertexCount() : INDEX_NONE;
	}

	inline UCambrianaGrammar* CreateGrammarInPackage(const TCHAR* PackageName)
	{
		UPackage* Package = CreatePackage(PackageName);
		UCambrianaGrammarFactory* Factory = NewObject<UCambrianaGrammarFactory>();
		return Cast<UCambrianaGrammar>(Factory->FactoryCreateNew(UCambrianaGrammar::StaticClass(), Package,
			*FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional, nullptr, nullptr));
	}

	inline bool UnloadPackage(UPackage* Package)
	{
		FText Error;
		return UPackageTools::UnloadPackages({ Package }, Error, true);
	}

	inline FString PackageFilename(const TCHAR* PackageName)
	{
		return FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()));
	}

	/**
	 * Runs an asset editor save so that it writes the package. With -unattended the engine's save prompt
	 * (FEditorFileUtils::PromptForCheckoutAndSave) cancels; for an unattended script it saves through
	 * UEditorLoadingAndSavingUtils::SavePackages instead.
	 */
	inline void RunWithoutSavePrompt(TFunctionRef<void()> Save)
	{
		const TGuardValue<bool> UnattendedScript(GIsRunningUnattendedScript, true);
		Save();
	}

	/** Runs Change between the pre- and post-edit notifications a Details edit of the property sends. */
	inline void EditProperty(UObject* Object, FName PropertyName, TFunctionRef<void()> Change)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		Object->PreEditChange(Property);
		Change();
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Object->PostEditChangeProperty(Event);
	}

	inline void SetGrammarText(UCambrianaGrammar* Grammar, const TCHAR* Text)
	{
		EditProperty(Grammar, GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText), [Grammar, Text] { Grammar->GrammarText = Text; });
	}

	/** Changes the grammar's sides as a Details edit does: one undoable property change. */
	inline void EditGrammarSides(UCambrianaGrammar* Grammar, int32 Sides)
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Edit Sides")));
		EditProperty(Grammar, GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Sides), [Grammar, Sides] { Grammar->Sides = Sides; });
	}

	inline const TCHAR* const TypedGrammarText = TEXT("generations 2\naxiom A\n\ngrow {\n    A -> F A\n}\n");

	/** Types TypedGrammarText over the whole Rules text, in several insertions as typing does. */
	inline void TypeOverRules(SMultiLineEditableTextBox& Rules)
	{
		Rules.SelectAllText();
		Rules.InsertTextAtCursor(FString(TEXT("generations 2\naxiom A\n\ngrow {\n")));
		Rules.InsertTextAtCursor(FString(TEXT("    A -> F A\n")));
		Rules.InsertTextAtCursor(FString(TEXT("}\n")));
	}

	inline void PauseTyping(float Seconds)
	{
		FTSTicker::GetCoreTicker().Tick(Seconds);
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

	inline TSharedPtr<SWidget> RulesTabWidgetOfType(FCambrianaGrammarEditorToolkit& Toolkit, FName Type)
	{
		const TSharedPtr<SDockTab> Tab = Toolkit.GetTabManager()->FindExistingLiveTab(FTabId(FCambrianaGrammarEditorToolkit::RulesTabId));
		return Tab ? FindWidgetOfType(Tab->GetContent(), Type) : nullptr;
	}

	inline TSharedPtr<SMultiLineEditableTextBox> RulesTextBoxOf(FCambrianaGrammarEditorToolkit& Toolkit)
	{
		return StaticCastSharedPtr<SMultiLineEditableTextBox>(RulesTabWidgetOfType(Toolkit, TEXT("SMultiLineEditableTextBox")));
	}

	inline TSharedRef<IPropertyRowGenerator> MakeRows(const TArray<UObject*>& Objects)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		const TSharedRef<IPropertyRowGenerator> Rows = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Rows->SetObjects(Objects);
		return Rows;
	}

	inline TSharedPtr<IDetailTreeNode> FindCategory(const IPropertyRowGenerator& Rows, FName Category)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Rows.GetRootTreeNodes())
		{
			if (Node->GetNodeType() == EDetailNodeType::Category && Node->GetNodeName() == Category)
			{
				return Node;
			}
		}
		return nullptr;
	}

	/** The number a parameter row's value field shows; NaN when the row has no numeric field. */
	inline double ShownValue(const TSharedPtr<IDetailTreeNode>& Row)
	{
		const TSharedPtr<SWidget> Value = Row ? Row->CreateNodeWidgets().ValueWidget : nullptr;
		if (Value)
		{
			// Bound Slate attributes refresh in the prepass of each frame, which a widget outside a window never gets.
			Value->SlatePrepass(1.0f);
		}
		const TSharedPtr<SWidget> Field = Value ? FindWidgetOfType(Value.ToSharedRef(), TEXT("SEditableText")) : nullptr;
		const FString Text = Field ? StaticCastSharedPtr<SEditableText>(Field)->GetText().ToString() : FString();
		return Text.IsNumeric() ? FCString::Atod(*Text) : std::numeric_limits<double>::quiet_NaN();
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

	inline FString DescribeSizeAndSurfacer(const UCambrianaGrammar& Grammar)
	{
		return FString::Printf(TEXT("size %s %g, sides %d, surface %s, min thickness %g"), Grammar.SizeMode == ECambrianaSizeMode::Fit ? TEXT("Fit") : TEXT("Step"),
			Grammar.SizeValue, Grammar.Sides, Grammar.bClosedForm ? TEXT("tubes") : TEXT("blocks"), Grammar.MinThickness);
	}

	inline FString LoadFile(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
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

	/** The node, or one nested in it, for the property; bIgnoreVisibility also looks in nodes the customization hides. */
	inline TSharedPtr<IDetailTreeNode> FindNode(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, FName PropertyName, bool bIgnoreVisibility = false)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() && Handle->GetProperty()->GetFName() == PropertyName)
			{
				return Node;
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, bIgnoreVisibility);
			if (TSharedPtr<IDetailTreeNode> Found = FindNode(Children, PropertyName, bIgnoreVisibility))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** A live actor in the editor world, referencing Grammar and holding InlineText as its inline grammar. */
	inline ACambrianaLiveActor* SpawnLiveActor(UCambrianaGrammar* Grammar, const TCHAR* InlineText)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		ACambrianaLiveActor* Actor = World ? World->SpawnActor<ACambrianaLiveActor>() : nullptr;
		if (Actor)
		{
			Actor->Grammar = Grammar;
			Actor->InlineGrammar = InlineText;
		}
		return Actor;
	}

	inline ACambrianaLiveActor* SpawnInlineActor(const TCHAR* InlineText)
	{
		return SpawnLiveActor(nullptr, InlineText);
	}

	inline void DestroyActor(ACambrianaLiveActor* Actor)
	{
		if (Actor)
		{
			Actor->GetWorld()->EditorDestroyActor(Actor, false);
		}
	}

	/** Closes the grammar's editor, destroys its actor and unloads its package. */
	inline void DiscardGrammarAndActor(UCambrianaGrammar* Grammar, ACambrianaLiveActor* Actor)
	{
		CloseGrammarEditor(Grammar);
		DestroyActor(Actor);
		UnloadPackage(Grammar->GetPackage());
	}

	/** Closes the grammar's editor and unloads its package. */
	inline void DiscardGrammar(UCambrianaGrammar* Grammar)
	{
		CloseGrammarEditor(Grammar);
		UnloadPackage(Grammar->GetPackage());
	}

	/** Answers the toolkit's unsaved-edits question with Answer, counting the times it is asked. */
	inline void AnswerUnsavedEdits(FCambrianaGrammarEditorToolkit& Toolkit, EAppReturnType::Type Answer, int32& Asked)
	{
		Toolkit.SetUnsavedEditsPrompt([&Asked, Answer](EAppMsgType::Type, const FText&) { ++Asked; return Answer; });
	}

	/** Answers the toolkit's Save As dialog with PackageName (empty cancels), counting the times it is asked. */
	inline void AnswerSaveAs(FCambrianaGrammarEditorToolkit& Toolkit, const FString& PackageName, int32& Asked)
	{
		Toolkit.SetSaveAsPrompt([&Asked, PackageName](const UCambrianaGrammar&) { ++Asked; return PackageName; });
	}

	/** The number of steps in the editor's undo history. */
	inline int32 UndoHistoryLength()
	{
		return GEditor->Trans->GetQueueLength();
	}

	/**
	 * Counts the grammar change broadcasts of Watched (of any grammar when null) while it lives, and whether an undo transaction was
	 * active for any. With bModify it also modifies the changed grammar, as a listener that edits an object would.
	 */
	class FGrammarChangeCounter
	{
	public:
		explicit FGrammarChangeCounter(const UCambrianaGrammar* InWatched = nullptr, bool bInModify = false)
			: Watched(InWatched)
			, bModify(bInModify)
		{
			Handle = UCambrianaGrammar::OnGrammarChanged().AddLambda([this](const UCambrianaGrammar* Changed)
			{
				if (Watched && Changed != Watched)
				{
					return;
				}
				++Count;
				bUndoActive |= GUndo != nullptr;
				if (bModify)
				{
					const_cast<UCambrianaGrammar*>(Changed)->Modify();
				}
			});
		}

		~FGrammarChangeCounter() { UCambrianaGrammar::OnGrammarChanged().Remove(Handle); }

		FGrammarChangeCounter(const FGrammarChangeCounter&) = delete;
		FGrammarChangeCounter& operator=(const FGrammarChangeCounter&) = delete;

		int32 Count = 0;
		bool bUndoActive = false;

	private:
		const UCambrianaGrammar* Watched;
		bool bModify;
		FDelegateHandle Handle;
	};

	/** Types Text over the whole Rules text in one insertion; TypeOverRules types the default text in several, as typing does. */
	inline void TypeText(SMultiLineEditableTextBox& Rules, const TCHAR* Text)
	{
		Rules.SelectAllText();
		Rules.InsertTextAtCursor(FString(Text));
	}
}
