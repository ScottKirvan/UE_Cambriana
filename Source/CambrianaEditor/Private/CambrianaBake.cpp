#include "CambrianaBake.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CambrianaAssetImportData.h"
#include "CambrianaBuild.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaPhysicsAsset.h"
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
#include "PhysicsEngine/PhysicsAsset.h"
#include "ToolMenus.h"
#include "UObject/ObjectRedirector.h"
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

	/**
	 * The object at an output's name, loading its package when only the package file exists. A redirector there is returned, not
	 * followed, as the engine's import finds the existing object in the destination package (AssetTools.cpp, ImportAssetsInternal,
	 * with AssetTools.FollowRedirectorsWhenImporting off by default) and as CanCreateAsset does.
	 */
	UObject* FindExisting(const FString& PackageName, const FString& Name)
	{
		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (!Package && FPackageName::DoesPackageExist(PackageName))
		{
			Package = LoadPackage(nullptr, *PackageName, LOAD_NoWarn | LOAD_Quiet);
		}
		return Package ? StaticFindObject(UObject::StaticClass(), Package, *Name) : nullptr;
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
		/** The asset's name, when it is not the package's short name. */
		FString AssetName;

		static FOutputToBake At(const UObject& Asset, UClass* Class)
		{
			return FOutputToBake{ Asset.GetPackage()->GetName(), Class, nullptr, Asset.GetName() };
		}

		FString Name() const { return AssetName.IsEmpty() ? FPackageName::GetShortName(PackageName) : AssetName; }

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

	/** The assets a bake writes: a static mesh, a skeletal mesh with its skeleton and physics asset, or all four. */
	struct FOutputsToBake
	{
		TOptional<FOutputToBake> StaticMesh;
		TOptional<FOutputToBake> Skeleton;
		TOptional<FOutputToBake> SkeletalMesh;
		TOptional<FOutputToBake> PhysicsAsset;
		/** The physics asset's overall stiffness (FCambrianaBakeSettings::PhysicsStiffness). */
		float PhysicsStiffness = 0.0f;
		/** The grammar the baked mesh is linked to for Reimport, or none. */
		const UCambrianaGrammar* Grammar = nullptr;
		/** How the skeleton gets the skeletal mesh's bones. */
		Cambriana::ECambrianaSkeletonUpdate SkeletonUpdate = Cambriana::ECambrianaSkeletonUpdate::RecreateBoneTree;
		/** Asks before an existing skeleton the bones cannot merge into is recreated; unset, a message dialog asks. */
		Cambriana::Editor::FAskToRecreateSkeleton AskToRecreateSkeleton;
		/** Whether the outputs are saved; otherwise they are left with unsaved changes. */
		bool bSave = true;

		TArray<FOutputToBake*, TInlineAllocator<4>> All()
		{
			TArray<FOutputToBake*, TInlineAllocator<4>> Outputs;
			for (TOptional<FOutputToBake>* Output : { &SkeletalMesh, &Skeleton, &PhysicsAsset, &StaticMesh })
			{
				if (Output->IsSet())
				{
					Outputs.Add(&Output->GetValue());
				}
			}
			return Outputs;
		}
	};

	/** Finds the object at each output's name; false, logged, when one is a redirector or an asset of another class. */
	bool FindExistingOutputs(FOutputsToBake& Outputs, const FString& Source)
	{
		for (FOutputToBake* Output : Outputs.All())
		{
			Output->Existing = FindExisting(Output->PackageName, Output->Name());
			if (const UObjectRedirector* Redirector = Cast<UObjectRedirector>(Output->Existing))
			{
				UE_LOG(LogCambrianaBake, Error, TEXT("%s: %s is a redirector to %s; nothing is baked"), *Source, *Redirector->GetPathName(),
					Redirector->DestinationObject ? *Redirector->DestinationObject->GetPathName() : TEXT("nothing"));
				return false;
			}
			if (Output->Existing && !Output->Existing->IsA(Output->Class))
			{
				UE_LOG(LogCambrianaBake, Error, TEXT("%s: %s is not a %s; nothing is baked"), *Source, *Output->Existing->GetPathName(),
					*FName::NameToDisplayString(Output->Class->GetName(), false).ToLower());
				return false;
			}
		}
		return true;
	}

	/** Whether the build has something to bake into the outputs; false, logged, when it does not. */
	bool HasSomethingToBake(const FCambrianaBuildResult& Build, const FOutputsToBake& Outputs, const FString& Source)
	{
		if (Build.Report.bHasErrors)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the grammar has errors; nothing is baked"), *Source);
			return false;
		}
		if (Build.Mesh.TriangleCount() == 0)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the grammar builds no triangles; nothing is baked"), *Source);
			return false;
		}
		if (Outputs.SkeletalMesh && Cambriana::MakeReferenceSkeleton(Build).GetRawBoneNum() == 0)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the grammar's bone tree makes no skeleton; nothing is baked"), *Source);
			return false;
		}
		return true;
	}

	/** Asks once, naming the first existing output, before existing outputs are rebuilt in place; true when none exists or the answer is Yes. */
	bool ConfirmOverwrite(FOutputsToBake& Outputs, const FString& Source, const Cambriana::Editor::FAskToOverwrite& AskToOverwrite)
	{
		const TArray<FOutputToBake*, TInlineAllocator<4>> All = Outputs.All();
		FOutputToBake* const* FirstExisting = All.FindByPredicate([](const FOutputToBake* Output) { return Output->Existing != nullptr; });
		if (!FirstExisting)
		{
			return true;
		}
		const FText Question = FText::Format(
			LOCTEXT("Overwrite", "Do you want to overwrite the existing asset?\n\nAn asset already exists at the bake location: {0}"),
			FText::FromString((*FirstExisting)->PackageName));
		if ((AskToOverwrite ? AskToOverwrite(Question) : AskWithDialog(Question)) == EAppReturnType::Yes)
		{
			return true;
		}
		UE_LOG(LogCambrianaBake, Display, TEXT("%s: %s is not overwritten; nothing is baked"), *Source, *(*FirstExisting)->Existing->GetPathName());
		return false;
	}

	/**
	 * Whether the build's bones merge into the skeleton, decided as USkeleton::MergeBonesToBoneTree decides it (Skeleton.cpp): an
	 * empty bone tree is created from them, and otherwise they merge when IsCompatibleMesh accepts a mesh with their reference
	 * skeleton, its parent chain check off only under a.Skeleton.AllowIncompatibleSkeletalMeshMerge.
	 */
	bool BonesMerge(const USkeleton& Skeleton, const FCambrianaBuildResult& Build)
	{
		if (Skeleton.GetReferenceSkeleton().GetRawBoneNum() == 0)
		{
			return true;
		}
		USkeletalMesh* Probe = NewObject<USkeletalMesh>(GetTransientPackage());
		Probe->SetRefSkeleton(Cambriana::MakeReferenceSkeleton(Build, &Skeleton));
		const IConsoleVariable* AllowIncompatible = IConsoleManager::Get().FindConsoleVariable(TEXT("a.Skeleton.AllowIncompatibleSkeletalMeshMerge"));
		const bool bMerge = Skeleton.IsCompatibleMesh(Probe, !(AllowIncompatible && AllowIncompatible->GetBool()));
		Probe->MarkAsGarbage();
		return bMerge;
	}

	/**
	 * For bones to merge into an existing skeleton that they cannot merge into, asks whether to recreate its bone tree, as the FBX
	 * import asks (FbxSkeletalMeshImport.cpp), before anything is rebuilt; true when they merge or the answer is Yes, which recreates
	 * it. Unset, a message dialog asks, answering Cancel when unattended.
	 */
	bool ConfirmSkeletonUpdate(const FCambrianaBuildResult& Build, FOutputsToBake& Outputs, const FString& Source)
	{
		const USkeleton* Skeleton = Outputs.Skeleton ? Cast<USkeleton>(Outputs.Skeleton->Existing) : nullptr;
		if (Outputs.SkeletonUpdate != Cambriana::ECambrianaSkeletonUpdate::MergeBones || !Skeleton || BonesMerge(*Skeleton, Build))
		{
			return true;
		}
		// The FBX import's question, under its namespace and key so that it shares its translations.
		const FText Question = NSLOCTEXT("FBXImpoter", "SkeletonFailed_BoneMerge", "FAILED TO MERGE BONES:\n\n This could happen if significant hierarchical changes have been made\ne.g. inserting a bone between nodes.\nWould you like to regenerate the Skeleton from this mesh?\n\n***WARNING: THIS MAY INVALIDATE OR REQUIRE RECOMPRESSION OF ANIMATION DATA.***\n");
		const EAppReturnType::Type Answer = Outputs.AskToRecreateSkeleton ? Outputs.AskToRecreateSkeleton(Question)
			: FMessageDialog::Open(EAppMsgType::YesNoCancel, EAppReturnType::Cancel, Question);
		if (Answer == EAppReturnType::Yes)
		{
			Outputs.SkeletonUpdate = Cambriana::ECambrianaSkeletonUpdate::RecreateBoneTree;
			return true;
		}
		UE_LOG(LogCambrianaBake, Display, TEXT("%s: the grammar's bones do not merge into %s, which is not recreated; nothing is rebuilt"), *Source,
			*Skeleton->GetPathName());
		return false;
	}

	/** Builds Build into the outputs, linking the baked mesh to the outputs' grammar, and marks their packages dirty; Packages gets them. */
	FCambrianaBakeResult WriteOutputs(const FCambrianaBuildResult& Build, const FOutputsToBake& Outputs, TArray<UPackage*>& Packages)
	{
		FCambrianaBakeResult Result{ ECambrianaBakeStatus::Baked };
		if (Outputs.SkeletalMesh)
		{
			Result.Skeleton = Outputs.Skeleton->MakeAsset<USkeleton>();
			Result.SkeletalMesh = Outputs.SkeletalMesh->MakeAsset<USkeletalMesh>();
			Cambriana::SetSkeletalMeshFromBuild(*Result.SkeletalMesh, *Result.Skeleton, Build, Outputs.SkeletonUpdate);
			UCambrianaAssetImportData::Link(*Result.SkeletalMesh, Outputs.Grammar);
			Packages.Add(Result.SkeletalMesh->GetPackage());
			Packages.Add(Result.Skeleton->GetPackage());
			if (Outputs.PhysicsAsset)
			{
				Result.PhysicsAsset = Outputs.PhysicsAsset->MakeAsset<UPhysicsAsset>();
				Cambriana::Editor::SetPhysicsAssetFromBuild(*Result.PhysicsAsset, *Result.SkeletalMesh, Build, Outputs.PhysicsStiffness);
				Packages.Add(Result.PhysicsAsset->GetPackage());
			}
		}
		if (Outputs.StaticMesh)
		{
			Result.StaticMesh = Outputs.StaticMesh->MakeAsset<UStaticMesh>();
			Cambriana::SetStaticMeshFromBuild(*Result.StaticMesh, Build);
			UCambrianaAssetImportData::Link(*Result.StaticMesh, Outputs.Grammar);
			Packages.Add(Result.StaticMesh->GetPackage());
		}
		for (UPackage* Package : Packages)
		{
			Package->MarkPackageDirty();
		}
		return Result;
	}

	/** Saves the written packages; the result becomes SaveFailed, logged, when they do not all save. */
	void SaveOutputs(TArray<UPackage*>& Packages, const FString& Source, FCambrianaBakeResult& Result)
	{
		if (FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false) != FEditorFileUtils::PR_Success)
		{
			UE_LOG(LogCambrianaBake, Error, TEXT("%s: the baked assets are built but not saved"), *Source);
			Result.Status = ECambrianaBakeStatus::SaveFailed;
			return;
		}
		for (const UPackage* Package : Packages)
		{
			UE_LOG(LogCambrianaBake, Display, TEXT("%s: baked %s"), *Source, *Package->GetName());
		}
	}

	/**
	 * Builds Input into the outputs and saves them unless the outputs say not to, asking once before any existing output is rebuilt in place; Source names what is
	 * baked in the log. A redirector or an asset of another class at an output's name bakes nothing.
	 */
	FCambrianaBakeResult BakeOutputs(const FCambrianaBuildInput& Input, FOutputsToBake Outputs, const FString& Source,
		const Cambriana::Editor::FAskToOverwrite& AskToOverwrite)
	{
		if (!FindExistingOutputs(Outputs, Source))
		{
			return Failed(ECambrianaBakeStatus::NameTaken);
		}
		const FCambrianaBuildResult Build = Cambriana::BuildMesh(Input);
		if (!HasSomethingToBake(Build, Outputs, Source))
		{
			return Failed(ECambrianaBakeStatus::NothingToBuild);
		}
		if (!ConfirmOverwrite(Outputs, Source, AskToOverwrite))
		{
			return Failed(ECambrianaBakeStatus::OverwriteDeclined);
		}
		if (!ConfirmSkeletonUpdate(Build, Outputs, Source))
		{
			return Failed(ECambrianaBakeStatus::SkeletonRecreateDeclined);
		}
		TArray<UPackage*> Packages;
		FCambrianaBakeResult Result = WriteOutputs(Build, Outputs, Packages);
		if (Outputs.bSave)
		{
			SaveOutputs(Packages, Source, Result);
		}
		return Result;
	}

	/** The outputs Choice writes, at Asset's names in the folder beside it, with State's bake settings. */
	FOutputsToBake OutputsBesideGrammar(const UCambrianaGrammar& Asset, const UCambrianaGrammar& State, ECambrianaBakeChoice Choice)
	{
		FOutputsToBake Outputs;
		if (Choice == ECambrianaBakeChoice::SkeletalMesh)
		{
			Outputs.SkeletalMesh = FOutputToBake{ OutputPackageName(Asset, TEXT("SK_")), USkeletalMesh::StaticClass() };
			Outputs.Skeleton = FOutputToBake{ OutputPackageName(Asset, TEXT("SKEL_")), USkeleton::StaticClass() };
			Outputs.PhysicsAsset = FOutputToBake{ OutputPackageName(Asset, TEXT("PHYS_")), UPhysicsAsset::StaticClass() };
			Outputs.PhysicsStiffness = State.Bake.PhysicsStiffness;
		}
		else
		{
			Outputs.StaticMesh = FOutputToBake{ OutputPackageName(Asset, TEXT("SM_")), UStaticMesh::StaticClass() };
		}
		return Outputs;
	}

	/** Bakes State, as it is in memory, into the outputs, linking the baked mesh to the grammar asset Asset. */
	FCambrianaBakeResult BakeGrammarTo(const UCambrianaGrammar& Asset, const UCambrianaGrammar& State, FOutputsToBake Outputs,
		const Cambriana::Editor::FAskToOverwrite& AskToOverwrite)
	{
		Outputs.Grammar = &Asset;
		return BakeOutputs(ACambrianaLiveActor::MakeBuildInputFor(State), MoveTemp(Outputs), Asset.GetPathName(), AskToOverwrite);
	}
	void ExecuteBake(const FToolMenuContext& MenuContext, ECambrianaBakeChoice Choice)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (!Context)
		{
			return;
		}
		for (UCambrianaGrammar* Grammar : Context->LoadSelectedObjects<UCambrianaGrammar>())
		{
			Cambriana::Editor::NotifyBakeResult(Grammar->GetPathName(), Cambriana::Editor::BakeGrammar(*Grammar, Choice));
		}
	}
}

namespace Cambriana::Editor
{
	FCambrianaBakeResult BakeGrammar(UCambrianaGrammar& Grammar, ECambrianaBakeChoice Choice, const FAskToOverwrite& AskToOverwrite)
	{
		return BakeGrammarState(Grammar, Grammar, Choice, AskToOverwrite);
	}

	FCambrianaBakeResult BakeGrammarState(const UCambrianaGrammar& Asset, const UCambrianaGrammar& State, ECambrianaBakeChoice Choice,
		const FAskToOverwrite& AskToOverwrite)
	{
		return BakeGrammarTo(Asset, State, OutputsBesideGrammar(Asset, State, Choice), AskToOverwrite);
	}

	FCambrianaBakeResult RebakeMesh(UCambrianaGrammar& Grammar, UObject& Mesh, const FAskToRecreateSkeleton& AskToRecreateSkeleton)
	{
		FOutputsToBake Outputs;
		if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(&Mesh))
		{
			Outputs = OutputsBesideGrammar(Grammar, Grammar, ECambrianaBakeChoice::SkeletalMesh);
			Outputs.SkeletalMesh = FOutputToBake::At(*SkeletalMesh, USkeletalMesh::StaticClass());
			// As the FBX reimport keeps the mesh's skeleton and merges the bones into it (FbxSkeletalMeshImport.cpp), or into the
			// skeleton it finds or makes at its own name when the mesh has none.
			if (const USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
			{
				Outputs.Skeleton = FOutputToBake::At(*Skeleton, USkeleton::StaticClass());
			}
			Outputs.SkeletonUpdate = Cambriana::ECambrianaSkeletonUpdate::MergeBones;
			Outputs.AskToRecreateSkeleton = AskToRecreateSkeleton;
			// As the FBX reimport keeps the mesh's physics asset and makes none (EditorFactories.cpp, UReimportFbxSkeletalMeshFactory,
			// bCreatePhysicsAsset false): only the grammar's own physics asset is rebuilt.
			const UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
			if (!PhysicsAsset || PhysicsAsset->GetPackage()->GetName() != Outputs.PhysicsAsset->PackageName || PhysicsAsset->GetName() != Outputs.PhysicsAsset->Name())
			{
				Outputs.PhysicsAsset.Reset();
			}
		}
		else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(&Mesh))
		{
			Outputs.StaticMesh = FOutputToBake::At(*StaticMesh, UStaticMesh::StaticClass());
		}
		else
		{
			return Failed(ECambrianaBakeStatus::NothingToBuild);
		}
		// Reimport rebuilds without asking and leaves the rebuilt assets unsaved, as the engine's FBX reimport does
		// (EditorFactories.cpp, UReimportFbxStaticMeshFactory::Reimport, which marks the package dirty).
		Outputs.bSave = false;
		return BakeGrammarTo(Grammar, Grammar, MoveTemp(Outputs), [](const FText&) { return EAppReturnType::Yes; });
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

	FText BakeResultMessage(const FString& Source, const FCambrianaBakeResult& Result)
	{
		const UObject* Baked = Result.SkeletalMesh ? static_cast<const UObject*>(Result.SkeletalMesh) : Result.StaticMesh;
		if (Result.Status == ECambrianaBakeStatus::Baked && Baked)
		{
			return FText::Format(LOCTEXT("Baked", "Baked {0}"), FText::FromString(Baked->GetPathName()));
		}
		if (Result.Status == ECambrianaBakeStatus::SaveFailed)
		{
			return FText::Format(LOCTEXT("BakedNotSaved", "{0} was baked but not saved; see the Output Log"), FText::FromString(Source));
		}
		return FText::Format(LOCTEXT("NotBaked", "{0} was not baked; see the Output Log"), FText::FromString(Source));
	}

	void NotifyBakeResult(const FString& Source, const FCambrianaBakeResult& Result)
	{
		if (Result.Status == ECambrianaBakeStatus::OverwriteDeclined || Result.Status == ECambrianaBakeStatus::LocationNotChosen
			|| Result.Status == ECambrianaBakeStatus::SkeletonRecreateDeclined || !FSlateApplication::IsInitialized())
		{
			return;
		}
		const bool bBaked = Result.Status == ECambrianaBakeStatus::Baked;
		FNotificationInfo Info(BakeResultMessage(Source, Result));
		Info.ExpireDuration = 5.0f;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bBaked ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	void AddBakeChoiceEntry(FToolMenuSection& Section, FName Name, ECambrianaBakeChoice Choice, FExecuteBakeChoice Execute)
	{
		const bool bStatic = Choice == ECambrianaBakeChoice::StaticMesh;
		Section.AddMenuEntry(Name,
			bStatic ? LOCTEXT("BakeToStaticMeshLabel", "Bake to Static Mesh") : LOCTEXT("BakeToSkeletalMeshLabel", "Bake to Skeletal Mesh"),
			bStatic ? LOCTEXT("BakeToStaticMeshTooltip", "Writes a static mesh from the grammar's current state to a folder beside it.")
				: LOCTEXT("BakeToSkeletalMeshTooltip", "Writes a skeletal mesh, skeleton and physics asset from the grammar's current state to a folder beside it."),
			FSlateIcon(), FToolMenuExecuteAction::CreateStatic(Execute, Choice));
	}

	void AddBakeMenuEntry()
	{
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UCambrianaGrammar::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection(BakeMenuSection);
		AddBakeChoiceEntry(Section, BakeToStaticMeshMenuEntry, ECambrianaBakeChoice::StaticMesh, &ExecuteBake);
		AddBakeChoiceEntry(Section, BakeToSkeletalMeshMenuEntry, ECambrianaBakeChoice::SkeletalMesh, &ExecuteBake);
	}
}

#undef LOCTEXT_NAMESPACE
