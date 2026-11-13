#include "CambrianaBake.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CambrianaBuild.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaSkeletalMesh.h"
#include "CambrianaStaticMesh.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Engine/SkeletalMesh.h"
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

	FString OutputPackageName(const UCambrianaGrammar& Grammar, const TCHAR* Prefix)
	{
		return BakeFolder(Grammar) / (Prefix + Grammar.GetName());
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

	Cambriana::Editor::FBakeLocationPrompt& BakeLocationPrompt()
	{
		static Cambriana::Editor::FBakeLocationPrompt Prompt;
		return Prompt;
	}

	/** The package the live actor's static mesh goes to, from the engine's save-asset dialog; empty when it is cancelled. */
	FString AskForBakeLocation(const FString& DefaultPackageName)
	{
		if (BakeLocationPrompt())
		{
			return BakeLocationPrompt()(DefaultPackageName);
		}
		const FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		const FString ObjectPath = ContentBrowser.Get().CreateModalSaveAssetDialog(Cambriana::Editor::MakeBakeLocationDialogConfig(DefaultPackageName));
		return ObjectPath.IsEmpty() ? FString() : FPackageName::ObjectPathToPackageName(ObjectPath);
	}

	FCambrianaBakeResult Failed(ECambrianaBakeStatus Status)
	{
		return { Status };
	}

	/** An asset a bake writes: its package and class, and the asset already at its name, if any. */
	struct FOutputToBake
	{
		FString PackageName;
		UClass* Class = nullptr;
		UObject* Existing = nullptr;

		FString Name() const { return FPackageName::GetShortName(PackageName); }

		/** The existing asset rebuilt in place, or a new one. */
		template <typename T>
		T* MakeAsset() const
		{
			if (T* Asset = Cast<T>(Existing))
			{
				return Asset;
			}
			T* Asset = NewObject<T>(CreatePackage(*PackageName), *Name(), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
			return Asset;
		}
	};

	/** The assets a bake writes: a static mesh, a skeleton and skinned skeletal mesh, or all three. */
	struct FOutputsToBake
	{
		TOptional<FOutputToBake> StaticMesh;
		TOptional<FOutputToBake> Skeleton;
		TOptional<FOutputToBake> SkeletalMesh;

		TArray<FOutputToBake*, TInlineAllocator<3>> All()
		{
			TArray<FOutputToBake*, TInlineAllocator<3>> Outputs;
			for (TOptional<FOutputToBake>* Output : { &SkeletalMesh, &Skeleton, &StaticMesh })
			{
				if (Output->IsSet())
				{
					Outputs.Add(&Output->GetValue());
				}
			}
			return Outputs;
		}
	};

	/**
	 * Builds Input into the outputs and saves them, asking once before any existing output is rebuilt in place; Source names what is
	 * baked in the log. An asset of another class at an output's name bakes nothing.
	 */
	FCambrianaBakeResult BakeOutputs(const FCambrianaBuildInput& Input, FOutputsToBake Outputs, const FString& Source,
		const Cambriana::Editor::FAskToOverwrite& AskToOverwrite)
	{
		const FOutputToBake* FirstExisting = nullptr;
		for (FOutputToBake* Output : Outputs.All())
		{
			Output->Existing = FindExisting(Output->PackageName, Output->Name());
			if (Output->Existing && !Output->Existing->IsA(Output->Class))
			{
				UE_LOG(LogCambrianaBake, Error, TEXT("%s: %s is not a %s; nothing is baked"), *Source, *Output->Existing->GetPathName(),
					*FName::NameToDisplayString(Output->Class->GetName(), false).ToLower());
				return Failed(ECambrianaBakeStatus::NameTaken);
			}
			FirstExisting = FirstExisting ? FirstExisting : (Output->Existing ? Output : nullptr);
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
		if (Outputs.SkeletalMesh && Cambriana::MakeReferenceSkeleton(Build).GetRawBoneNum() == 0)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the grammar's bone tree makes no skeleton; nothing is baked"), *Source);
			return Failed(ECambrianaBakeStatus::NothingToBuild);
		}

		if (FirstExisting)
		{
			const FText Question = FText::Format(
				LOCTEXT("Overwrite", "Do you want to overwrite the existing asset?\n\nAn asset already exists at the bake location: {0}"),
				FText::FromString(FirstExisting->PackageName));
			if ((AskToOverwrite ? AskToOverwrite(Question) : AskWithDialog(Question)) != EAppReturnType::Yes)
			{
				UE_LOG(LogCambrianaBake, Display, TEXT("%s: %s is not overwritten; nothing is baked"), *Source, *FirstExisting->Existing->GetPathName());
				return Failed(ECambrianaBakeStatus::OverwriteDeclined);
			}
		}

		FCambrianaBakeResult Result{ ECambrianaBakeStatus::Baked };
		TArray<UPackage*> Packages;
		if (Outputs.SkeletalMesh)
		{
			Result.Skeleton = Outputs.Skeleton->MakeAsset<USkeleton>();
			Result.SkeletalMesh = Outputs.SkeletalMesh->MakeAsset<USkeletalMesh>();
			Cambriana::SetSkeletalMeshFromBuild(*Result.SkeletalMesh, *Result.Skeleton, Build);
			Packages.Add(Result.SkeletalMesh->GetPackage());
			Packages.Add(Result.Skeleton->GetPackage());
		}
		if (Outputs.StaticMesh)
		{
			Result.StaticMesh = Outputs.StaticMesh->MakeAsset<UStaticMesh>();
			Cambriana::SetStaticMeshFromBuild(*Result.StaticMesh, Build);
			Packages.Add(Result.StaticMesh->GetPackage());
		}
		for (UPackage* Package : Packages)
		{
			Package->MarkPackageDirty();
		}

		if (FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false) != FEditorFileUtils::PR_Success)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the baked assets are built but not saved"), *Source);
			Result.Status = ECambrianaBakeStatus::SaveFailed;
			return Result;
		}
		for (UPackage* Package : Packages)
		{
			UE_LOG(LogCambrianaBake, Display, TEXT("%s: baked %s"), *Source, *Package->GetName());
		}
		return Result;
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
		if (!EnsureSaved(Grammar))
		{
			UE_LOG(LogCambrianaBake, Warning, TEXT("%s: the grammar is not saved; nothing is baked"), *GrammarPath);
			return Failed(ECambrianaBakeStatus::GrammarNotSaved);
		}

		FOutputsToBake Outputs;
		Outputs.SkeletalMesh = FOutputToBake{ OutputPackageName(Grammar, TEXT("SK_")), USkeletalMesh::StaticClass() };
		Outputs.Skeleton = FOutputToBake{ OutputPackageName(Grammar, TEXT("SKEL_")), USkeleton::StaticClass() };
		if (Grammar.Bake.bStaticMesh)
		{
			Outputs.StaticMesh = FOutputToBake{ OutputPackageName(Grammar, TEXT("SM_")), UStaticMesh::StaticClass() };
		}
		return BakeOutputs(ACambrianaLiveActor::MakeBuildInputFor(Grammar), MoveTemp(Outputs), GrammarPath, AskToOverwrite);
	}

	FSaveAssetDialogConfig MakeBakeLocationDialogConfig(const FString& DefaultPackageName)
	{
		FSaveAssetDialogConfig Config;
		Config.DialogTitleOverride = LOCTEXT("BakeLocationTitle", "Bake Static Mesh");
		Config.DefaultPath = FPackageName::GetLongPackagePath(DefaultPackageName);
		Config.DefaultAssetName = FPackageName::GetShortName(DefaultPackageName);
		Config.AssetClassNames = { UStaticMesh::StaticClass()->GetClassPathName() };
		Config.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		return Config;
	}

	FBakeLocationPrompt SetBakeLocationPrompt(FBakeLocationPrompt Prompt)
	{
		FBakeLocationPrompt Previous = MoveTemp(BakeLocationPrompt());
		BakeLocationPrompt() = MoveTemp(Prompt);
		return Previous;
	}

	FCambrianaBakeResult BakeLiveActor(ACambrianaLiveActor& Actor)
	{
		const FString ActorPath = Actor.GetPathName();
		if (Actor.Grammar && !EnsureSaved(*Actor.Grammar))
		{
			UE_LOG(LogCambrianaBake, Warning, TEXT("%s: the grammar is not saved; nothing is baked"), *ActorPath);
			return Failed(ECambrianaBakeStatus::GrammarNotSaved);
		}
		const FString PackageName = AskForBakeLocation(StaticMeshPackageName(Actor));
		if (PackageName.IsEmpty())
		{
			UE_LOG(LogCambrianaBake, Display, TEXT("%s: no location is chosen; nothing is baked"), *ActorPath);
			return Failed(ECambrianaBakeStatus::LocationNotChosen);
		}
		// The dialog has already asked before replacing an existing static mesh (SAssetDialog, AllowButWarn).
		FOutputsToBake Outputs;
		Outputs.StaticMesh = FOutputToBake{ PackageName, UStaticMesh::StaticClass() };
		return BakeOutputs(Actor.MakeBuildInput(), MoveTemp(Outputs), ActorPath, [](const FText&) { return EAppReturnType::Yes; });
	}

	void NotifyBakeResult(const FString& Source, const FCambrianaBakeResult& Result)
	{
		if (Result.Status == ECambrianaBakeStatus::OverwriteDeclined || Result.Status == ECambrianaBakeStatus::LocationNotChosen
			|| !FSlateApplication::IsInitialized())
		{
			return;
		}
		const bool bBaked = Result.Status == ECambrianaBakeStatus::Baked;
		const UObject* Baked = Result.SkeletalMesh ? static_cast<const UObject*>(Result.SkeletalMesh) : Result.StaticMesh;
		FNotificationInfo Info(bBaked && Baked
			? FText::Format(LOCTEXT("Baked", "Baked {0}"), FText::FromString(Baked->GetPathName()))
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
			LOCTEXT("BakeTooltip", "Writes a skeletal mesh and skeleton, and a static mesh when selected, from the grammar's saved state to a folder beside it."), FSlateIcon(),
			FToolMenuExecuteAction::CreateStatic(&ExecuteBake));
	}
}

#undef LOCTEXT_NAMESPACE
