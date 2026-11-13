#pragma once

#include "Animation/Skeleton.h"
#include "CambrianaBake.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"

namespace Cambriana::Editor::Tests
{
	inline const TCHAR* const BakeTestRoot = TEXT("/Game/CambrianaBakeTests");

	/** A grammar asset in its own package under BakeTestRoot, holding sk.ls with static mesh output on; removes it and whatever was baked from it, in memory and on disk. */
	class FBakeFixture
	{
	public:
		explicit FBakeFixture(const TCHAR* InName)
			: Name(InName)
			, GrammarPackage(FString(BakeTestRoot) / InName)
			// Expected location, from SDD-unreal.md: a folder beside the grammar asset, the asset named SM_<grammar asset name>.
			, MeshPackage(FString(BakeTestRoot) / InName / (TEXT("SM_") + FString(InName)))
			// SDD-unreal.md: the skeletal mesh and skeleton are SK_ and SKEL_ plus the grammar asset's name, in the same folder.
			, SkeletalMeshPackage(FString(BakeTestRoot) / InName / (TEXT("SK_") + FString(InName)))
			, SkeletonPackage(FString(BakeTestRoot) / InName / (TEXT("SKEL_") + FString(InName)))
		{
			Remove();
			Grammar = CreateGrammarInPackage(*GrammarPackage);
			Grammar->GrammarText = LoadFile(FixturePath(TEXT("sk.ls")));
			Grammar->Bake.bStaticMesh = true;
		}

		~FBakeFixture() { Remove(); }

		FBakeFixture(const FBakeFixture&) = delete;
		FBakeFixture& operator=(const FBakeFixture&) = delete;

		FString MeshObjectPath() const { return MeshPackage + TEXT(".SM_") + Name; }

		UStaticMesh* FindMesh() const { return FindObject<UStaticMesh>(nullptr, *MeshObjectPath()); }

		bool MeshFileExists() const { return IFileManager::Get().FileExists(*PackageFilename(*MeshPackage)); }

		USkeletalMesh* FindSkeletalMesh() const
		{
			return FindObject<USkeletalMesh>(nullptr, *(SkeletalMeshPackage + TEXT(".SK_") + Name));
		}

		USkeleton* FindSkeleton() const { return FindObject<USkeleton>(nullptr, *(SkeletonPackage + TEXT(".SKEL_") + Name)); }

		bool SkeletalFilesExist() const
		{
			return IFileManager::Get().FileExists(*PackageFilename(*SkeletalMeshPackage))
				&& IFileManager::Get().FileExists(*PackageFilename(*SkeletonPackage));
		}

		bool AnySkeletalFileExists() const
		{
			return IFileManager::Get().FileExists(*PackageFilename(*SkeletalMeshPackage))
				|| IFileManager::Get().FileExists(*PackageFilename(*SkeletonPackage));
		}

		/** Bakes with the engine's save prompts saving without asking, as for an unattended script. */
		FCambrianaBakeResult Bake(const Cambriana::Editor::FAskToOverwrite& AskToOverwrite = nullptr) const
		{
			FCambrianaBakeResult Result;
			RunWithoutSavePrompt([&] { Result = Cambriana::Editor::BakeGrammar(*Grammar, AskToOverwrite); });
			return Result;
		}

		/** Changes the grammar as an edit does, so that the bake saves it first. */
		void Edit(TFunctionRef<void(UCambrianaGrammar&)> Change) const
		{
			Change(*Grammar);
			Grammar->MarkPackageDirty();
		}

		const FString Name;
		const FString GrammarPackage;
		const FString MeshPackage;
		const FString SkeletalMeshPackage;
		const FString SkeletonPackage;
		UCambrianaGrammar* Grammar = nullptr;

	private:
		void Remove()
		{
			for (const FString& PackageName : { MeshPackage, SkeletalMeshPackage, SkeletonPackage, GrammarPackage })
			{
				if (UPackage* Package = FindPackage(nullptr, *PackageName))
				{
					if (UCambrianaGrammar* Loaded = FindObject<UCambrianaGrammar>(Package, *Name))
					{
						CloseGrammarEditor(Loaded);
					}
					UnloadPackage(Package);
				}
				IFileManager::Get().Delete(*PackageFilename(*PackageName));
			}
			IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename(*MeshPackage)), false, true);
			IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename(*GrammarPackage)), false, false);
			Grammar = nullptr;
		}
	};
}
