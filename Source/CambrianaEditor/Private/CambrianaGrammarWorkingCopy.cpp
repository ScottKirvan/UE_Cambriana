#include "CambrianaGrammarWorkingCopy.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CambrianaGrammar.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/PackageName.h"
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

#if WITH_EDITORONLY_DATA
	/** The file's absolute path, its record resolved against its grammar's package as the engine resolves it; empty for a cleared file. */
	FString ResolvedFilename(const FAssetImportInfo::FSourceFile& File, const UCambrianaGrammar& Owner)
	{
		return File.RelativeFilename.IsEmpty() ? FString() : UAssetImportData::ResolveImportFilename(File.RelativeFilename, Owner.GetPackage());
	}

	bool SameSourceFiles(const UCambrianaGrammar& A, const UCambrianaGrammar& B)
	{
		if (!A.AssetImportData || !B.AssetImportData)
		{
			return !A.AssetImportData == !B.AssetImportData;
		}
		const TArray<FAssetImportInfo::FSourceFile>& FilesA = A.AssetImportData->SourceData.SourceFiles;
		const TArray<FAssetImportInfo::FSourceFile>& FilesB = B.AssetImportData->SourceData.SourceFiles;
		if (FilesA.Num() != FilesB.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < FilesA.Num(); ++Index)
		{
			const FAssetImportInfo::FSourceFile& FileA = FilesA[Index];
			const FAssetImportInfo::FSourceFile& FileB = FilesB[Index];
			if (!ResolvedFilename(FileA, A).Equals(ResolvedFilename(FileB, B), ESearchCase::CaseSensitive) || FileA.Timestamp != FileB.Timestamp
				|| FileA.FileHash != FileB.FileHash || !FileA.DisplayLabelName.Equals(FileB.DisplayLabelName, ESearchCase::CaseSensitive))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Copies the source file record into To's own import data, each path recorded for To's package as an import records it. A path
	 * To already records for the same file stays as To records it, so a record the engine cannot resolve survives a copy there and back.
	 */
	void CopySourceFiles(const UCambrianaGrammar& From, UCambrianaGrammar& To)
	{
		if (!From.AssetImportData || !To.AssetImportData)
		{
			return;
		}
		FAssetImportInfo Copied = From.AssetImportData->SourceData;
		const TArray<FAssetImportInfo::FSourceFile>& Existing = To.AssetImportData->SourceData.SourceFiles;
		for (int32 Index = 0; Index < Copied.SourceFiles.Num(); ++Index)
		{
			FAssetImportInfo::FSourceFile& File = Copied.SourceFiles[Index];
			const FString Absolute = ResolvedFilename(File, From);
			if (Absolute.IsEmpty())
			{
				continue;
			}
			const bool bSameFile = Existing.IsValidIndex(Index) && ResolvedFilename(Existing[Index], To).Equals(Absolute, ESearchCase::CaseSensitive);
			File.RelativeFilename = bSameFile ? Existing[Index].RelativeFilename : UAssetImportData::SanitizeImportFilename(Absolute, To.GetPackage());
		}
		To.AssetImportData->SourceData = MoveTemp(Copied);
	}
#endif
}

namespace Cambriana::Editor
{
	UCambrianaGrammar* DuplicateWorkingCopy(const UCambrianaGrammar& Grammar)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UCambrianaGrammar::StaticClass(), *(Grammar.GetName() + TEXT("_WorkingCopy")));
		UCambrianaGrammar* WorkingCopy = CastChecked<UCambrianaGrammar>(
			StaticDuplicateObject(&Grammar, GetTransientPackage(), Name, ~(RF_Standalone | RF_Public)));
		WorkingCopy->SetFlags(RF_Transactional);
#if WITH_EDITORONLY_DATA
		CopySourceFiles(Grammar, *WorkingCopy);
#endif
		return WorkingCopy;
	}

	bool HoldSameGrammar(const UCambrianaGrammar& A, const UCambrianaGrammar& B)
	{
		bool bSame = true;
		ForEachGrammarProperty([&](const FProperty& Property)
		{
			bSame = bSame && SameValue(Property, A, B);
		});
#if WITH_EDITORONLY_DATA
		bSame = bSame && SameSourceFiles(A, B);
#endif
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
		CopyGrammarValues(From, To);
		NotifyGrammarChanged(To);
		return true;
	}

	bool CopyGrammarValues(const UCambrianaGrammar& From, UCambrianaGrammar& To)
	{
		if (HoldSameGrammar(From, To))
		{
			return false;
		}
		ForEachGrammarProperty([&](const FProperty& Property)
		{
			Property.CopyCompleteValue_InContainer(&To, &From);
		});
#if WITH_EDITORONLY_DATA
		CopySourceFiles(From, To);
#endif
		return true;
	}

	void NotifyGrammarChanged(UCambrianaGrammar& Grammar)
	{
		const TGuardValue<ITransaction*> OutsideUndoHistory(GUndo, nullptr);
		FPropertyChangedEvent Event(nullptr);
		Grammar.PostEditChangeProperty(Event);
	}

	UCambrianaGrammar* DuplicateAsAsset(const UCambrianaGrammar& Grammar, const FString& LongPackageName)
	{
		UPackage* Package = CreatePackage(*LongPackageName);
		UCambrianaGrammar* Asset = Cast<UCambrianaGrammar>(StaticDuplicateObject(&Grammar, Package, *FPackageName::GetLongPackageAssetName(LongPackageName)));
		if (!Asset)
		{
			return nullptr;
		}
		Asset->ClearFlags(RF_Transient);
		Asset->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
#if WITH_EDITORONLY_DATA
		CopySourceFiles(Grammar, *Asset);
#endif
		Asset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}
}
