#include "CambrianaBake.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CambrianaBuild.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaStaticMesh.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogCambrianaBake, Log, All);

#define LOCTEXT_NAMESPACE "CambrianaBake"

namespace
{
	/** The folder a grammar's baked assets go to: beside the grammar, named after it. */
	FString BakeFolder(const UCambrianaGrammar& Grammar)
	{
		return FPackageName::GetLongPackagePath(Grammar.GetPackage()->GetName()) / Grammar.GetName();
	}

	FString StaticMeshPackageName(const UCambrianaGrammar& Grammar)
	{
		return BakeFolder(Grammar) / (TEXT("SM_") + Grammar.GetName());
	}

	FString StaticMeshPackageName(const ACambrianaLiveActor& Actor)
	{
		const FString Label = ObjectTools::SanitizeObjectName(Actor.GetActorLabel());
		return Actor.Grammar
			? BakeFolder(*Actor.Grammar) / (TEXT("SM_") + Actor.Grammar->GetName() + TEXT("_") + Label)
			: FString(TEXT("/Game")) / (TEXT("SM_") + Label);
	}

	bool HasSavedState(const UPackage& Package)
	{
		return !Package.IsDirty() && FPackageName::DoesPackageExist(Package.GetName());
	}

	/** Whether the grammar has a saved state matching its values, offering it to the engine's save prompt when it does not. */
	bool EnsureSaved(UCambrianaGrammar& Grammar)
	{
		UPackage* Package = Grammar.GetPackage();
		if (Package == GetTransientPackage())
		{
			return false;
		}
		if (!HasSavedState(*Package))
		{
			FEditorFileUtils::PromptForCheckoutAndSave({ Package }, false, true);
		}
		return HasSavedState(*Package);
	}

	/** The asset at the static mesh's name, loading it when only its package file exists. */
	UObject* FindExisting(const FString& PackageName, const FString& Name)
	{
		const FString ObjectPath = PackageName + TEXT(".") + Name;
		if (UObject* Loaded = FindObject<UObject>(nullptr, *ObjectPath))
		{
			return Loaded;
		}
		return FPackageName::DoesPackageExist(PackageName) ? LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	}

	EAppReturnType::Type AskWithDialog(const FText& Question)
	{
		return FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Question);
	}

	FCambrianaBakeResult Failed(ECambrianaBakeStatus Status)
	{
		return { Status, nullptr };
	}

	/** Builds Input into the static mesh at PackageName and saves it, asking before an existing one is rebuilt; Source names what is baked in the log. */
	FCambrianaBakeResult BakeStaticMesh(const FCambrianaBuildInput& Input, const FString& PackageName, const FString& Source, const Cambriana::Editor::FAskToOverwrite& AskToOverwrite)
	{
		const FString Name = FPackageName::GetShortName(PackageName);
		UObject* Existing = FindExisting(PackageName, Name);
		UStaticMesh* ExistingMesh = Cast<UStaticMesh>(Existing);
		if (Existing && !ExistingMesh)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: %s is not a static mesh; nothing is baked"), *Source, *Existing->GetPathName());
			return Failed(ECambrianaBakeStatus::NameTaken);
		}

		const FCambrianaBuildResult Build = Cambriana::BuildMesh(Input);
		if (Build.Report.bHasErrors)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the grammar has errors; nothing is baked"), *Source);
			return Failed(ECambrianaBakeStatus::NothingToBuild);
		}
		if (Build.Mesh.TriangleCount() == 0)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the grammar builds no triangles; nothing is baked"), *Source);
			return Failed(ECambrianaBakeStatus::NothingToBuild);
		}

		if (ExistingMesh)
		{
			const FText Question = FText::Format(
				LOCTEXT("Overwrite", "Do you want to overwrite the existing asset?\n\nAn asset already exists at the bake location: {0}"),
				FText::FromString(PackageName));
			if ((AskToOverwrite ? AskToOverwrite(Question) : AskWithDialog(Question)) != EAppReturnType::Yes)
			{
				UE_LOG(LogCambrianaBake, Display, TEXT("%s: %s is not overwritten; nothing is baked"), *Source, *ExistingMesh->GetPathName());
				return Failed(ECambrianaBakeStatus::OverwriteDeclined);
			}
		}

		UStaticMesh* Mesh = ExistingMesh
			? ExistingMesh
			: NewObject<UStaticMesh>(CreatePackage(*PackageName), *Name, RF_Public | RF_Standalone | RF_Transactional);
		Cambriana::SetStaticMeshFromBuild(*Mesh, Build);
		Mesh->MarkPackageDirty();
		if (!ExistingMesh)
		{
			FAssetRegistryModule::AssetCreated(Mesh);
		}

		if (FEditorFileUtils::PromptForCheckoutAndSave({ Mesh->GetPackage() }, false, false) != FEditorFileUtils::PR_Success)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: %s is built but not saved"), *Source, *Mesh->GetPathName());
			return { ECambrianaBakeStatus::SaveFailed, Mesh };
		}
		UE_LOG(LogCambrianaBake, Display, TEXT("%s: baked %s"), *Source, *Mesh->GetPathName());
		return { ECambrianaBakeStatus::Baked, Mesh };
	}

	void ExecuteBake(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (!Context)
		{
			return;
		}
		for (UCambrianaGrammar* Grammar : Context->LoadSelectedObjects<UCambrianaGrammar>())
		{
			Cambriana::Editor::NotifyBakeResult(Grammar->GetPathName(), Cambriana::Editor::BakeGrammar(*Grammar));
		}
	}
}

namespace Cambriana::Editor
{
	FCambrianaBakeResult BakeGrammar(UCambrianaGrammar& Grammar, const FAskToOverwrite& AskToOverwrite)
	{
		const FString GrammarPath = Grammar.GetPathName();
		if (!Grammar.Bake.bStaticMesh)
		{
			UE_LOG(LogCambrianaBake, Warning, TEXT("%s: no bake output is selected; nothing is baked. Turn on Static Mesh in its Bake settings."), *GrammarPath);
			return Failed(ECambrianaBakeStatus::NoOutputSelected);
		}
		if (!EnsureSaved(Grammar))
		{
			UE_LOG(LogCambrianaBake, Warning, TEXT("%s: the grammar is not saved; nothing is baked"), *GrammarPath);
			return Failed(ECambrianaBakeStatus::GrammarNotSaved);
		}

		return BakeStaticMesh(ACambrianaLiveActor::MakeBuildInputFor(Grammar), StaticMeshPackageName(Grammar), GrammarPath, AskToOverwrite);
	}

	FSaveAssetDialogConfig MakeBakeLocationDialogConfig(const FString& DefaultPackageName)
	{
		return FSaveAssetDialogConfig();
	}

	FBakeLocationPrompt SetBakeLocationPrompt(FBakeLocationPrompt Prompt)
	{
		return nullptr;
	}

	FCambrianaBakeResult BakeLiveActor(ACambrianaLiveActor& Actor)
	{
		const FString ActorPath = Actor.GetPathName();
		if (Actor.Grammar && !EnsureSaved(*Actor.Grammar))
		{
			UE_LOG(LogCambrianaBake, Warning, TEXT("%s: the grammar is not saved; nothing is baked"), *ActorPath);
			return Failed(ECambrianaBakeStatus::GrammarNotSaved);
		}
		return BakeStaticMesh(Actor.MakeBuildInput(), StaticMeshPackageName(Actor), ActorPath, nullptr);
	}

	void NotifyBakeResult(const FString& Source, const FCambrianaBakeResult& Result)
	{
		if (Result.Status == ECambrianaBakeStatus::OverwriteDeclined || !FSlateApplication::IsInitialized())
		{
			return;
		}
		const bool bBaked = Result.Status == ECambrianaBakeStatus::Baked;
		FNotificationInfo Info(bBaked
			? FText::Format(LOCTEXT("Baked", "Baked {0}"), FText::FromString(Result.StaticMesh->GetPathName()))
			: FText::Format(LOCTEXT("NotBaked", "{0} was not baked; see the Output Log"), FText::FromString(Source)));
		Info.ExpireDuration = 5.0f;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bBaked ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	void AddBakeMenuEntry()
	{
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UCambrianaGrammar::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection(BakeMenuSection);
		Section.AddMenuEntry(BakeMenuEntry, LOCTEXT("BakeLabel", "Bake"),
			LOCTEXT("BakeTooltip", "Writes the grammar's selected bake outputs from its saved state to a folder beside it."), FSlateIcon(),
			FToolMenuExecuteAction::CreateStatic(&ExecuteBake));
	}
}

#undef LOCTEXT_NAMESPACE
