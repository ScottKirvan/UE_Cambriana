#include "CambrianaDetach.h"

#include "Algo/AnyOf.h"
#include "AssetRegistry/AssetData.h"
#include "CambrianaAssetImportData.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "CambrianaDetach"

namespace
{
	void Detach(UObject& Mesh)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(&Mesh))
		{
			UCambrianaAssetImportData::Link(*StaticMesh, nullptr);
		}
		else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(&Mesh))
		{
			UCambrianaAssetImportData::Link(*SkeletalMesh, nullptr);
		}
	}

	void ExecuteDetach(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			Cambriana::Editor::DetachFromGrammar(Context->LoadSelectedObjects<UObject>());
		}
	}

	// A dynamic entry in GetAssetActions decided from the selection's registry records without loading it, as the Content Browser
	// asks of its menus (ContentBrowserMenuContexts.h, GetSelectedObjects) and as the static mesh's Nanite entries decide
	// (AssetDefinition_StaticMesh.cpp). CADKernelRetessellateAction.cpp also reads a registry tag in its dynamic entry, but its
	// visibility check loads the selection. The selection is loaded only when the entry runs.
	void AddDetachEntryIfLinked(FToolMenuSection& Section)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(Section);
		if (!Context || !Algo::AnyOf(Context->SelectedAssets, &UCambrianaAssetImportData::MayLinkToGrammar))
		{
			return;
		}
		Section.AddMenuEntry(Cambriana::Editor::DetachFromGrammarMenuEntry, LOCTEXT("DetachFromGrammarLabel", "Detach from Grammar"),
			LOCTEXT("DetachFromGrammarTooltip", "Removes the mesh's link to the grammar it was baked from, so it can no longer be reimported from it."),
			FSlateIcon(), FToolMenuExecuteAction::CreateStatic(&ExecuteDetach));
	}
}

namespace Cambriana::Editor
{
	int32 DetachFromGrammar(TConstArrayView<UObject*> Meshes)
	{
		TArray<UObject*, TInlineAllocator<4>> Linked;
		for (UObject* Mesh : Meshes)
		{
			if (UCambrianaAssetImportData::Of(Mesh))
			{
				Linked.Add(Mesh);
			}
		}
		if (Linked.IsEmpty())
		{
			return 0;
		}
		// As the Source File row's Clear (AssetImportDataCustomization.cpp, FImportDataSourceFileTransactionScope): one transaction,
		// Modify before the change, the package marked dirty after. The mesh is modified since its import data object is replaced.
		const FScopedTransaction Transaction(LOCTEXT("DetachFromGrammarTransaction", "Detach from Grammar"));
		for (UObject* Mesh : Linked)
		{
			Mesh->Modify();
			Detach(*Mesh);
			Mesh->MarkPackageDirty();
		}
		return Linked.Num();
	}

	void AddDetachMenuEntry()
	{
		for (UClass* MeshClass : { UStaticMesh::StaticClass(), USkeletalMesh::StaticClass() })
		{
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(MeshClass);
			Menu->FindOrAddSection(DetachMenuSection).AddDynamicEntry(DetachFromGrammarDynamicEntry,
				FNewToolMenuSectionDelegate::CreateStatic(&AddDetachEntryIfLinked));
		}
	}
}

#undef LOCTEXT_NAMESPACE
