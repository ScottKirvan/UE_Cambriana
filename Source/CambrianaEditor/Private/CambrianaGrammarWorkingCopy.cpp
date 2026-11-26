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
	bool SameSourceFile(const FAssetImportInfo::FSourceFile& A, const FAssetImportInfo::FSourceFile& B)
	{
		return A.RelativeFilename.Equals(B.RelativeFilename, ESearchCase::CaseSensitive) && A.Timestamp == B.Timestamp && A.FileHash == B.FileHash
			&& A.DisplayLabelName.Equals(B.DisplayLabelName, ESearchCase::CaseSensitive);
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
			if (!SameSourceFile(FilesA[Index], FilesB[Index]))
			{
				return false;
			}
		}
		return true;
	}

	// Each grammar keeps its own import data object; only the source file record moves between them.
	void CopySourceFiles(const UCambrianaGrammar& From, UCambrianaGrammar& To)
	{
		if (From.AssetImportData && To.AssetImportData)
		{
			To.AssetImportData->SourceData = From.AssetImportData->SourceData;
		}
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
		Asset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}
}
