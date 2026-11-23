#include "Animation/SkeletalMeshActor.h"
#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "CambrianaAssetImportData.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaDetach.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "CambrianaReimportFactory.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentBrowserMenuContexts.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "InterchangeManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ToolMenus.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Cambriana::Editor::Tests;

namespace
{
	// FMT-grammar.md, Commands: an argument to F is a percentage of the step. Two bones straight up, the second len% of the 10 cm step.
	const TCHAR* const ShapeGrammarText = TEXT("step 10cm\nshape param len = 100\n\naxiom F F(len)\n");
	// Hand calculation: 10 cm + 10 cm at len 100, 10 cm + 5 cm at len 50, 10 cm + 2.5 cm at len 25. A tube has no caps
	// (SDD-surfacing.md, Tubes), so the mesh's height is the bones' length.
	constexpr double HeightAtFullLength = 20.0;
	constexpr double HeightAtHalfLength = 15.0;
	constexpr double HeightAtQuarterLength = 12.5;

	void SetLength(UCambrianaGrammar& Grammar, double Value)
	{
		Grammar.ParameterValues.RemoveAll([](const FCambrianaParameterValue& Stored) { return Stored.Name == TEXT("len"); });
		FCambrianaParameterValue& Stored = Grammar.ParameterValues.AddDefaulted_GetRef();
		Stored.Name = TEXT("len");
		Stored.Value = Value;
	}

	bool Save(UObject* Asset)
	{
		return UEditorLoadingAndSavingUtils::SavePackages({ Asset->GetPackage() }, false);
	}

	/**
	 * Runs the engine's Reimport on the asset as the Content Browser's does once its source file is known, without notifications. An
	 * asset no handler claims is not reimported, since the engine would ask for a file in a dialog. Reimport saves the rebuilt assets
	 * as Bake does, through the engine's save, which saves without asking only for an unattended script.
	 */
	bool Reimport(UObject* Asset)
	{
		bool bReimported = false;
		RunWithoutSavePrompt([&]
		{
			bReimported = FReimportManager::Instance()->CanReimport(Asset)
				&& FReimportManager::Instance()->Reimport(Asset, false, false, FString(), nullptr, INDEX_NONE, false, true);
		});
		return bReimported;
	}

	UCambrianaReimportFactory& Handler()
	{
		return *GetMutableDefault<UCambrianaReimportFactory>();
	}

	TArray<FString> ClaimedFiles(UObject* Asset)
	{
		TArray<FString> Files;
		return Handler().CanReimport(Asset, Files) ? Files : TArray<FString>();
	}

	double Height(const FBox& Box)
	{
		return Box.Max.Z - Box.Min.Z;
	}

	double MeshHeight(const UStaticMesh* Mesh)
	{
		return Mesh ? Height(Mesh->GetBoundingBox()) : -1.0;
	}

	bool SameFile(const FString& A, const FString& B)
	{
		return !A.IsEmpty() && FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(A), FPaths::ConvertRelativePathToFull(B));
	}

	FString PackageFileOf(const UObject* Asset)
	{
		return PackageFilename(*Asset->GetPackage()->GetName());
	}

	/** The source files the Content Browser's Reimport reads for the asset (AssetFileContextMenu.cpp, GetSelectedAssetSourceFilePaths). */
	TArray<FString> ContentBrowserSourceFiles(const UObject* Asset)
	{
		TArray<FString> Files;
		const FAssetData AssetData(Asset);
		if (const UAssetDefinition* Definition = UAssetDefinitionRegistry::Get()->GetAssetDefinitionForAsset(AssetData))
		{
			FAssetSourceFilesArgs Args;
			Args.Assets = TConstArrayView<FAssetData>(&AssetData, 1);
			Definition->GetSourceFiles(Args, [&Files](const FAssetSourceFilesResult& Result) { Files.Add(Result.FilePath); return true; });
		}
		return Files;
	}

	/** A second grammar asset in its own package under the bake test root; removes it in memory and on disk. */
	class FOtherGrammar
	{
	public:
		explicit FOtherGrammar(const TCHAR* Name)
			: PackageName(FString(BakeTestRoot) / Name)
		{
			Remove();
			Grammar = CreateGrammarInPackage(*PackageName);
			Grammar->GrammarText = ShapeGrammarText;
		}

		~FOtherGrammar() { Remove(); }

		FOtherGrammar(const FOtherGrammar&) = delete;
		FOtherGrammar& operator=(const FOtherGrammar&) = delete;

		FString Filename() const { return PackageFilename(*PackageName); }

		const FString PackageName;
		UCambrianaGrammar* Grammar = nullptr;

	private:
		void Remove()
		{
			if (UPackage* Package = FindPackage(nullptr, *PackageName))
			{
				UnloadPackage(Package);
			}
			IFileManager::Get().Delete(*Filename());
		}
	};

	/** The fixture's grammar holding ShapeGrammarText at len 100, saved. */
	bool PrepareShapeGrammar(FAutomationTestBase& Test, FBakeFixture& Fixture)
	{
		Fixture.Grammar->GrammarText = ShapeGrammarText;
		SetLength(*Fixture.Grammar, 100.0);
		return Test.TestTrue(TEXT("The grammar saves"), Save(Fixture.Grammar));
	}

	/** Changes the grammar's len and saves it, as ATS-43's change of a shape parameter does. */
	bool ChangeLengthAndSave(FAutomationTestBase& Test, UCambrianaGrammar& Grammar, double Value)
	{
		Grammar.Modify();
		SetLength(Grammar, Value);
		return Test.TestTrue(TEXT("The changed grammar saves"), Save(&Grammar));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeRecordsGrammarOnStaticMeshTest,
	"Cambriana.Editor.Reimport.BakeToStaticMeshRecordsTheGrammarAndItsPackageFileOnTheStaticMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeRecordsGrammarOnStaticMeshTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("RecordStatic"));
	// A grammar's package file resolves as the source file once it is on disk (UAssetImportData::ResolveImportFilename).
	if (!TestTrue(TEXT("The grammar saves"), Save(Fixture.Grammar)))
	{
		return false;
	}
	if (!TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked) || !TestNotNull(TEXT("SM_RecordStatic"), Fixture.FindMesh()))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	const FString GrammarFile = PackageFilename(*Fixture.GrammarPackage);

	// SDD-unreal.md: the import data is a UCambrianaAssetImportData holding a soft reference to the grammar, its source file the grammar's package file.
	const UCambrianaAssetImportData* ImportData = Cast<UCambrianaAssetImportData>(Mesh->GetAssetImportData());
	if (TestNotNull(TEXT("The static mesh's import data links its grammar"), ImportData))
	{
		TestTrue(TEXT("It references the grammar asset"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Fixture.Grammar));
		TestTrue(TEXT("Its source file is the grammar's package file"), SameFile(ImportData->GetFirstFilename(), GrammarFile));
		TestEqual(TEXT("It has one source file"), ImportData->GetSourceFileCount(), 1);
	}
	const TArray<FString> BrowserFiles = ContentBrowserSourceFiles(Mesh);
	TestTrue(TEXT("The Content Browser reads that source file, so it enables Reimport"), BrowserFiles.Num() == 1 && SameFile(BrowserFiles[0], GrammarFile));
	TArray<FString> ManagerFiles;
	TestTrue(TEXT("The engine's reimport manager can reimport it"), FReimportManager::Instance()->CanReimport(Mesh, &ManagerFiles));
	TestTrue(TEXT("The handler that claims it reads the grammar's package file"), ManagerFiles.Num() == 1 && SameFile(ManagerFiles[0], GrammarFile));
	const TArray<FString> Claimed = ClaimedFiles(Mesh);
	TestTrue(TEXT("The Cambriana reimport factory claims it, with the grammar's package file"), Claimed.Num() == 1 && SameFile(Claimed[0], GrammarFile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaBakeRecordsGrammarOnSkeletalMeshTest,
	"Cambriana.Editor.Reimport.BakeToSkeletalMeshRecordsTheGrammarOnTheSkeletalMeshAndNotOnItsSkeletonOrPhysicsAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaBakeRecordsGrammarOnSkeletalMeshTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("RecordSkeletal"));
	// A grammar's package file resolves as the source file once it is on disk (UAssetImportData::ResolveImportFilename).
	if (!TestTrue(TEXT("The grammar saves"), Save(Fixture.Grammar)))
	{
		return false;
	}
	if (!TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked)
		|| !TestNotNull(TEXT("SK_RecordSkeletal"), Fixture.FindSkeletalMesh()))
	{
		return false;
	}
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	const FString GrammarFile = PackageFilename(*Fixture.GrammarPackage);

	const UCambrianaAssetImportData* ImportData = Cast<UCambrianaAssetImportData>(Mesh->GetAssetImportData());
	if (TestNotNull(TEXT("The skeletal mesh's import data links its grammar"), ImportData))
	{
		TestTrue(TEXT("It references the grammar asset"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Fixture.Grammar));
		TestTrue(TEXT("Its source file is the grammar's package file"), SameFile(ImportData->GetFirstFilename(), GrammarFile));
	}
	const TArray<FString> BrowserFiles = ContentBrowserSourceFiles(Mesh);
	TestTrue(TEXT("The Content Browser reads that source file, so it enables Reimport"), BrowserFiles.Num() == 1 && SameFile(BrowserFiles[0], GrammarFile));
	const TArray<FString> Claimed = ClaimedFiles(Mesh);
	TestTrue(TEXT("The Cambriana reimport factory claims it, with the grammar's package file"), Claimed.Num() == 1 && SameFile(Claimed[0], GrammarFile));
	TestTrue(TEXT("It does not claim the skeleton"), ClaimedFiles(Fixture.FindSkeleton()).IsEmpty());
	TestTrue(TEXT("It does not claim the physics asset"), ClaimedFiles(Fixture.FindPhysicsAsset()).IsEmpty());
	TestNull(TEXT("No static mesh is baked to carry a link"), Fixture.FindMesh());
	return true;
}

// ATS-43, static mesh.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportStaticMeshTest,
	"Cambriana.Editor.Reimport.ReimportRebuildsTheStaticMeshInPlaceFromTheGrammarsCurrentStateAndThePlacedInstanceShowsIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportStaticMeshTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportStatic"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	TestEqual(TEXT("The baked mesh is 20 cm tall"), MeshHeight(Mesh), HeightAtFullLength, VertexTolerance);
	AStaticMeshActor* Placed = PlaceAsset<AStaticMeshActor>(Mesh);
	if (!TestNotNull(TEXT("The static mesh is placed in the level"), Placed) || !ChangeLengthAndSave(*this, *Fixture.Grammar, 50.0))
	{
		return false;
	}

	// With -unattended an overwrite question would answer No and stop the rebuild, so success means none was asked.
	const FDateTime MeshSavedAt = IFileManager::Get().GetTimeStamp(*PackageFileOf(Mesh));
	TestTrue(TEXT("Reimport succeeds without asking to overwrite"), Reimport(Mesh));

	TestTrue(TEXT("The same static mesh object is rebuilt"), Fixture.FindMesh() == Mesh);
	TestEqual(TEXT("It is rebuilt from the changed len of 50: 15 cm tall"), MeshHeight(Mesh), HeightAtHalfLength, VertexTolerance);
	// As the engine's FBX reimport, which marks the package dirty and does not save it.
	TestTrue(TEXT("It is left with unsaved changes"), Mesh->GetPackage()->IsDirty());
	TestTrue(TEXT("Its package file is not written"), IFileManager::Get().GetTimeStamp(*PackageFileOf(Mesh)) == MeshSavedAt);
	ACambrianaLiveActor* Live = PlaceInCurrentLevel(Fixture.Grammar);
	if (TestNotNull(TEXT("A live actor is placed from the grammar"), Live))
	{
		const FTriangleMatch Match = MatchTriangles(*Live->GetDynamicMeshComponent()->GetMesh(), *Mesh);
		TestTrue(FString::Printf(TEXT("The rebuilt mesh matches the live actor built from the changed grammar (%s)"), *Match.Describe()), Match.Matches());
	}
	UStaticMeshComponent* Component = Placed->GetStaticMeshComponent();
	TestTrue(TEXT("The placed instance still references the static mesh"), Component->GetStaticMesh() == Mesh);
	TestEqual(TEXT("The placed instance's bounds are the rebuilt mesh's: 15 cm tall"), Height(Component->Bounds.GetBox()), HeightAtHalfLength, VertexTolerance);

	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
	TestTrue(TEXT("The mesh keeps its link to the grammar"), ImportData && ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Fixture.Grammar));
	Placed->GetWorld()->EditorDestroyActor(Placed, false);
	DestroyActor(Live);
	return true;
}

// ATS-43, skeletal mesh.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportSkeletalMeshTest,
	"Cambriana.Editor.Reimport.ReimportRebuildsTheSkeletalMeshInPlaceKeepingItsSkeletonAndPhysicsAssetAndThePlacedInstanceShowsIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportSkeletalMeshTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportSkeletal"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	USkeleton* Skeleton = Fixture.FindSkeleton();
	UPhysicsAsset* PhysicsAsset = Fixture.FindPhysicsAsset();
	ASkeletalMeshActor* Placed = PlaceAsset<ASkeletalMeshActor>(Mesh);
	if (!TestNotNull(TEXT("The skeletal mesh is placed in the level"), Placed) || !ChangeLengthAndSave(*this, *Fixture.Grammar, 50.0))
	{
		return false;
	}
	USkeletalMeshComponent* Component = Placed->GetSkeletalMeshComponent();
	ACambrianaLiveActor* Live = PlaceInCurrentLevel(Fixture.Grammar);
	if (!TestNotNull(TEXT("A live actor is placed from the changed grammar"), Live))
	{
		return false;
	}
	const UE::Geometry::FDynamicMesh3& LiveMesh = *Live->GetDynamicMeshComponent()->GetMesh();
	const FTransform LiveFrame = Live->GetDynamicMeshComponent()->GetComponentTransform();
	TestFalse(TEXT("Before Reimport the placed instance does not match the changed grammar"),
		MatchTriangles(LiveMesh, SkinnedTrianglesOf(*Component, LiveFrame)).Matches());

	const FDateTime MeshSavedAt = IFileManager::Get().GetTimeStamp(*PackageFileOf(Mesh));
	TestTrue(TEXT("Reimport succeeds without asking to overwrite"), Reimport(Mesh));

	TestTrue(TEXT("The same skeletal mesh object is rebuilt"), Fixture.FindSkeletalMesh() == Mesh);
	TestTrue(TEXT("It keeps its skeleton, the same object"), Mesh->GetSkeleton() == Skeleton && Fixture.FindSkeleton() == Skeleton);
	TestTrue(TEXT("It keeps its physics asset, the same object"), Mesh->GetPhysicsAsset() == PhysicsAsset && Fixture.FindPhysicsAsset() == PhysicsAsset);
	TestTrue(TEXT("The skeletal mesh, skeleton and physics asset are left with unsaved changes"),
		Mesh->GetPackage()->IsDirty() && Skeleton->GetPackage()->IsDirty() && PhysicsAsset->GetPackage()->IsDirty());
	TestTrue(TEXT("The skeletal mesh's package file is not written"), IFileManager::Get().GetTimeStamp(*PackageFileOf(Mesh)) == MeshSavedAt);
	TestTrue(TEXT("The placed instance still references the skeletal mesh"), Component->GetSkeletalMeshAsset() == Mesh);
	// The editor poses a placed component on its next tick; the test does not tick, so it poses it as that tick would.
	Component->RefreshBoneTransforms();
	const FTriangleMatch Match = MatchTriangles(LiveMesh, SkinnedTrianglesOf(*Component, LiveFrame));
	TestTrue(FString::Printf(TEXT("The placed instance matches the live actor built from the changed grammar (%s)"), *Match.Describe()), Match.Matches());
	Placed->GetWorld()->EditorDestroyActor(Placed, false);
	DestroyActor(Live);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportNeverSavedGrammarTest,
	"Cambriana.Editor.Reimport.AMeshLinkedToANeverSavedGrammarIsClaimedWithThatGrammarsPackageFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportNeverSavedGrammarTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportNeverSaved"));
	FOtherGrammar Other(TEXT("ReimportNeverSavedOther"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
	if (!TestNotNull(TEXT("The mesh links its grammar"), ImportData))
	{
		return false;
	}
	ImportData->SetGrammar(*Other.Grammar);
	TestFalse(TEXT("The other grammar has never been saved"), IFileManager::Get().FileExists(*Other.Filename()));
	TestTrue(TEXT("The link names the never-saved grammar"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Other.Grammar));

	const TArray<FString> Claimed = ClaimedFiles(Mesh);
	TestTrue(TEXT("The mesh is claimed, with the never-saved grammar's package file"), Claimed.Num() == 1 && SameFile(Claimed[0], Other.Filename()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportMovedGrammarTest,
	"Cambriana.Editor.Reimport.AGrammarMovedAfterBakeIsFollowedByItsLinkAndReimportRefreshesTheSourceFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportMovedGrammarTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportMoved"));
	const FString MovedPath = FString(BakeTestRoot) / TEXT("ReimportMovedTo");
	const FString MovedPackage = MovedPath / TEXT("ReimportMovedRenamed");
	const FString MovedFile = PackageFilename(*MovedPackage);
	ON_SCOPE_EXIT
	{
		if (UPackage* Package = FindPackage(nullptr, *MovedPackage))
		{
			UnloadPackage(Package);
		}
		IFileManager::Get().Delete(*MovedFile);
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(MovedFile), false, true);
	};
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	UCambrianaGrammar* Grammar = Fixture.Grammar;

	// The editor's asset registry reads a saved package's dependencies when its directory watcher sees the file, on a later tick; the
	// test does not tick, so it reads the baked mesh's file now. The rename manager finds the mesh as a referencer from them.
	IAssetRegistry::GetChecked().ScanFilesSynchronous({ PackageFileOf(Mesh) }, true);
	TArray<FName> Dependencies;
	IAssetRegistry::GetChecked().GetDependencies(*Mesh->GetPackage()->GetName(), Dependencies);
	TestTrue(TEXT("The saved mesh's package records its grammar's package as a dependency"), Dependencies.Contains(FName(*Fixture.GrammarPackage)));
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	if (!TestTrue(TEXT("The grammar is moved and renamed"), AssetTools.RenameAssets({ FAssetRenameData(TWeakObjectPtr<UObject>(Grammar), MovedPath, FString(TEXT("ReimportMovedRenamed"))) })))
	{
		return false;
	}
	TestEqual(TEXT("The grammar asset is at its new path"), Grammar->GetPackage()->GetName(), MovedPackage);
	if (!ChangeLengthAndSave(*this, *Grammar, 50.0))
	{
		return false;
	}

	const TArray<FString> Claimed = ClaimedFiles(Mesh);
	TestTrue(TEXT("The mesh is still claimed, with the moved grammar's package file"), Claimed.Num() == 1 && SameFile(Claimed[0], MovedFile));
	TestTrue(TEXT("Reimport succeeds"), Reimport(Mesh));
	TestEqual(TEXT("The mesh is rebuilt from the moved grammar at len 50: 15 cm tall"), MeshHeight(Mesh), HeightAtHalfLength, VertexTolerance);
	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
	if (TestNotNull(TEXT("The mesh links its grammar"), ImportData))
	{
		TestTrue(TEXT("The link names the grammar's new path"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Grammar));
		TestTrue(TEXT("The source file is refreshed to the moved grammar's package file"), SameFile(ImportData->GetFirstFilename(), MovedFile));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportStaleSourceFileTest,
	"Cambriana.Editor.Reimport.ASourceFileChangedAwayFromTheGrammarIsRefreshedFromTheLinkOnReimport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportStaleSourceFileTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportStale"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
	if (!TestNotNull(TEXT("The mesh links its grammar"), ImportData))
	{
		return false;
	}
	// As the Source File row's browse button does (AssetImportDataCustomization.cpp, OnChangePathClicked).
	ImportData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("elsewhere.uasset")));
	const FString GrammarFile = PackageFilename(*Fixture.GrammarPackage);

	const TArray<FString> Claimed = ClaimedFiles(Mesh);
	TestTrue(TEXT("The mesh is claimed with the linked grammar's package file"), Claimed.Num() == 1 && SameFile(Claimed[0], GrammarFile));
	TestTrue(TEXT("Reimport succeeds"), Reimport(Mesh));
	TestTrue(TEXT("The source file is the linked grammar's package file again"), SameFile(ImportData->GetFirstFilename(), GrammarFile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportWithNewFileTest,
	"Cambriana.Editor.Reimport.ReimportWithNewFileRelinksToAGrammarPackageFileAndFailsForAnyOtherFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportWithNewFileTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is not a grammar asset's package file"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
	FBakeFixture Fixture(TEXT("ReimportNewFile"));
	FOtherGrammar Other(TEXT("ReimportNewFileOther"));
	const FString TextFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CambrianaReimportNewFile.txt"));
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*TextFile); };
	FFileHelper::SaveStringToFile(TEXT("not a grammar"), *TextFile);
	SetLength(*Other.Grammar, 25.0);
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The other grammar saves"), Save(Other.Grammar))
		|| !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	if (!TestEqual(TEXT("The Cambriana reimport factory claims the mesh"), ClaimedFiles(Mesh).Num(), 1))
	{
		return false;
	}

	// As Reimport With New File does once a file is chosen (EditorReimportHandler.h, FReimportManager::ValidateAllSourceFileAndReimport).
	FReimportManager::Instance()->UpdateReimportPath(Mesh, Other.Filename(), 0);
	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
	if (!TestNotNull(TEXT("The mesh links a grammar"), ImportData))
	{
		return false;
	}
	TestTrue(TEXT("A grammar's package file relinks the mesh to that grammar"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Other.Grammar));
	TestTrue(TEXT("Its source file is that package file"), SameFile(ImportData->GetFirstFilename(), Other.Filename()));
	TestTrue(TEXT("Reimport succeeds"), Reimport(Mesh));
	TestEqual(TEXT("The mesh is rebuilt from the other grammar at len 25: 12.5 cm tall"), MeshHeight(Mesh), HeightAtQuarterLength, VertexTolerance);

	for (const FString& NotAGrammar : { TextFile, PackageFileOf(Mesh) })
	{
		FReimportManager::Instance()->UpdateReimportPath(Mesh, NotAGrammar, 0);
		TestFalse(FString::Printf(TEXT("Reimport with %s fails"), *FPaths::GetCleanFilename(NotAGrammar)), Reimport(Mesh));
		TestTrue(TEXT("The mesh stays linked to the other grammar"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Other.Grammar));
		TestTrue(TEXT("Its source file stays that grammar's package file"), SameFile(ImportData->GetFirstFilename(), Other.Filename()));
	}
	TestEqual(TEXT("The mesh is unchanged: 12.5 cm tall"), MeshHeight(Mesh), HeightAtQuarterLength, VertexTolerance);
	TestTrue(TEXT("A later Reimport succeeds again"), Reimport(Mesh));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportClaimsOnlyLinkedMeshesTest,
	"Cambriana.Editor.Reimport.TheReimportFactoryClaimsOnlyMeshesLinkedToAGrammarThatResolvesAboveInterchange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportClaimsOnlyLinkedMeshesTest::RunTest(const FString& Parameters)
{
	// SDD-unreal.md: priority UFactory::DefaultImportPriority + 20, above Interchange's + 10 (InterchangeReimportHandler.cpp).
	TestEqual(TEXT("The handler's priority"), Handler().GetPriority(), UFactory::GetDefaultImportPriority() + 20);

	UStaticMesh* Plain = NewObject<UStaticMesh>(GetTransientPackage());
	TestTrue(TEXT("A static mesh without import data is not claimed"), ClaimedFiles(Plain).IsEmpty());

	UStaticMesh* FromFbx = NewObject<UStaticMesh>(GetTransientPackage());
	UAssetImportData* FbxData = NewObject<UAssetImportData>(FromFbx);
	FbxData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("tree.fbx")));
	FromFbx->SetAssetImportData(FbxData);
	TestTrue(TEXT("A static mesh imported from an FBX file is not claimed"), ClaimedFiles(FromFbx).IsEmpty());

	USkeletalMesh* SkeletalFromFbx = NewObject<USkeletalMesh>(GetTransientPackage());
	UAssetImportData* SkeletalFbxData = NewObject<UAssetImportData>(SkeletalFromFbx);
	SkeletalFbxData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("tree.fbx")));
	SkeletalFromFbx->SetAssetImportData(SkeletalFbxData);
	TestTrue(TEXT("A skeletal mesh imported from an FBX file is not claimed"), ClaimedFiles(SkeletalFromFbx).IsEmpty());

	UStaticMesh* Unresolved = NewObject<UStaticMesh>(GetTransientPackage());
	UCambrianaAssetImportData* UnresolvedData = NewObject<UCambrianaAssetImportData>(Unresolved);
	UnresolvedData->Grammar = TSoftObjectPtr<UCambrianaGrammar>(FSoftObjectPath(TEXT("/Game/CambrianaBakeTests/Nowhere/Missing.Missing")));
	Unresolved->SetAssetImportData(UnresolvedData);
	TestTrue(TEXT("A mesh linked to a grammar that does not resolve is not claimed"), ClaimedFiles(Unresolved).IsEmpty());

	UStaticMesh* LinkedToOther = NewObject<UStaticMesh>(GetTransientPackage());
	UCambrianaAssetImportData* LinkedData = NewObject<UCambrianaAssetImportData>(LinkedToOther);
	LinkedData->Grammar = TSoftObjectPtr<UCambrianaGrammar>(FSoftObjectPath(LinkedToOther));
	LinkedToOther->SetAssetImportData(LinkedData);
	TestTrue(TEXT("A mesh whose link names an asset that is not a grammar is not claimed"), ClaimedFiles(LinkedToOther).IsEmpty());

	TestTrue(TEXT("A grammar asset is not claimed"), ClaimedFiles(CreateGrammarWithFactory()).IsEmpty());
	TestFalse(TEXT("The factory imports no file"), Handler().FactoryCanImport(PackageFilename(TEXT("/Game/CambrianaBakeTests/Any"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportInterchangeTest,
	"Cambriana.Editor.Reimport.InterchangeLeavesTheGrammarLinkAloneAndDoesNotClaimTheBakedMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportInterchangeTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportInterchange"));
	if (!TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UInterchangeManager& Interchange = UInterchangeManager::GetInterchangeManager();
	for (UObject* Mesh : { static_cast<UObject*>(Fixture.FindMesh()), static_cast<UObject*>(Fixture.FindSkeletalMesh()) })
	{
		const UCambrianaAssetImportData* Before = UCambrianaAssetImportData::Of(Mesh);
		if (!TestNotNull(TEXT("The baked mesh links its grammar"), Before))
		{
			continue;
		}
		// FReimportManager::ReimportAsync converts the import data for the reimport file's extension before every reimport (Editor.cpp).
		TestFalse(FString::Printf(TEXT("%s: Interchange converts nothing for a .uasset source file"), *Mesh->GetName()),
			Interchange.ConvertImportData(Mesh, FPaths::GetExtension(Before->GetFirstFilename()).ToLower()));
		TestTrue(TEXT("The mesh keeps the same import data"), UCambrianaAssetImportData::Of(Mesh) == Before);
		TArray<FString> Files;
		TestFalse(FString::Printf(TEXT("%s: Interchange cannot reimport it"), *Mesh->GetName()), Interchange.CanReimport(Mesh, Files));
	}
	return true;
}

// ATS-43: Reimport rebuilds from the grammar's current state, unsaved changes included.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaReimportCurrentStateTest,
	"Cambriana.Editor.Reimport.ReimportRebuildsFromTheGrammarsUnsavedChangesWithoutSavingTheGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaReimportCurrentStateTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportCurrent"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	Fixture.Edit([](UCambrianaGrammar& Grammar) { SetLength(Grammar, 50.0); });
	const FDateTime SavedAt = IFileManager::Get().GetTimeStamp(*PackageFilename(*Fixture.GrammarPackage));

	TestTrue(TEXT("Reimport succeeds"), Reimport(Mesh));

	TestEqual(TEXT("The mesh is rebuilt from the unsaved len of 50: 15 cm tall"), MeshHeight(Mesh), HeightAtHalfLength, VertexTolerance);
	TestTrue(TEXT("The grammar keeps its unsaved changes"), Fixture.Grammar->GetPackage()->IsDirty());
	TestTrue(TEXT("The grammar's package file is not written"), IFileManager::Get().GetTimeStamp(*PackageFilename(*Fixture.GrammarPackage)) == SavedAt);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaLiveActorBakeClearsLinkTest,
	"Cambriana.Editor.Reimport.ALiveActorBakeOverAMeshLinkedToAGrammarClearsItsImportData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaLiveActorBakeClearsLinkTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportActorBake"));
	if (!TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestNotNull(TEXT("The baked mesh links its grammar"), UCambrianaAssetImportData::Of(Fixture.FindMesh())))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	ACambrianaLiveActor* Actor = PlaceInCurrentLevel(Fixture.Grammar);
	if (!TestNotNull(TEXT("A live actor is placed from the grammar"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT { DestroyActor(Actor); };
	Actor->bOverrideSides = true;
	Actor->Sides = 6;
	Actor->Rebuild();

	const FString MeshPackage = Fixture.MeshPackage;
	Cambriana::Editor::FBakeLocationPrompt Previous = Cambriana::Editor::SetBakeLocationPrompt([MeshPackage](const FString&) { return MeshPackage; });
	ON_SCOPE_EXIT { Cambriana::Editor::SetBakeLocationPrompt(MoveTemp(Previous)); };
	FCambrianaBakeResult Result;
	RunWithoutSavePrompt([&] { Result = Cambriana::Editor::BakeLiveActor(*Actor); });

	TestTrue(TEXT("The live actor bakes over the grammar's static mesh"), Result.Status == ECambrianaBakeStatus::Baked && Result.StaticMesh == Mesh);
	const UAssetImportData* ImportData = Mesh->GetAssetImportData();
	// SDD-unreal.md: as Detach does, a plain UAssetImportData with no source file.
	TestTrue(TEXT("Its import data is a plain UAssetImportData"), ImportData && ImportData->GetClass() == UAssetImportData::StaticClass());
	TestEqual(TEXT("It has no source file"), ImportData ? ImportData->GetSourceFileCount() : -1, 0);
	TestTrue(TEXT("The Cambriana reimport factory no longer claims it"), ClaimedFiles(Mesh).IsEmpty());
	TestTrue(TEXT("The Content Browser reads no source file, so Reimport is disabled"), ContentBrowserSourceFiles(Mesh).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaEditorBakeLinksAssetTest,
	"Cambriana.Editor.Reimport.TheGrammarEditorsBakeLinksTheMeshToTheGrammarAssetNotTheWorkingCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaEditorBakeLinksAssetTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("ReimportEditorBake"));
	// A grammar's package file resolves as the source file once it is on disk (UAssetImportData::ResolveImportFilename).
	if (!TestTrue(TEXT("The grammar saves"), Save(Fixture.Grammar)))
	{
		return false;
	}
	FCambrianaGrammarEditorToolkit* Toolkit = OpenGrammarEditor(Fixture.Grammar);
	if (!TestNotNull(TEXT("The grammar opens in the grammar editor"), Toolkit))
	{
		return false;
	}
	EditGrammarSides(Toolkit->GetWorkingCopy(), 6);
	RunWithoutSavePrompt([Toolkit] { Toolkit->Bake(ECambrianaBakeChoice::StaticMesh); });

	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Fixture.FindMesh());
	if (TestNotNull(TEXT("The baked mesh links a grammar"), ImportData))
	{
		TestTrue(TEXT("The link is the grammar asset"), ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Fixture.Grammar));
		TestTrue(TEXT("Its source file is the asset's package file"), SameFile(ImportData->GetFirstFilename(), PackageFilename(*Fixture.GrammarPackage)));
	}
	return true;
}

namespace
{
	/**
	 * The Detach from Grammar entry in the asset actions of the right-click menu of MeshClass, generated for the selection as the
	 * Content Browser generates it; unset when none is offered. Context is the menu context to run it with.
	 */
	TOptional<FToolMenuEntry> OfferedDetachEntryForAssets(UClass* MeshClass, TConstArrayView<FAssetData> Selected, FToolMenuContext& Context)
	{
		const UToolMenu* Registered = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(MeshClass);
		if (!Registered)
		{
			return {};
		}
		UContentBrowserAssetContextMenuContext* AssetContext = NewObject<UContentBrowserAssetContextMenuContext>();
		AssetContext->SelectedAssets = TArray<FAssetData>(Selected);
		AssetContext->CommonClass = MeshClass;
		AssetContext->bCanBeModified = true;
		Context = FToolMenuContext(AssetContext);
		UToolMenu* Menu = UToolMenus::Get()->GenerateMenu(Registered->GetMenuName(), Context);
		FToolMenuSection* Section = Menu ? Menu->FindSection(Cambriana::Editor::DetachMenuSection) : nullptr;
		const FToolMenuEntry* Entry = Section ? Section->FindEntry(Cambriana::Editor::DetachFromGrammarMenuEntry) : nullptr;
		return Entry ? TOptional<FToolMenuEntry>(*Entry) : TOptional<FToolMenuEntry>();
	}

	TOptional<FToolMenuEntry> OfferedDetachEntry(UClass* MeshClass, TConstArrayView<UObject*> Selected, FToolMenuContext& Context)
	{
		TArray<FAssetData> Assets;
		for (UObject* Asset : Selected)
		{
			Assets.Add(FAssetData(Asset));
		}
		return OfferedDetachEntryForAssets(MeshClass, Assets, Context);
	}

	bool IsDetachOffered(UClass* MeshClass, TConstArrayView<UObject*> Selected)
	{
		FToolMenuContext Context;
		return OfferedDetachEntry(MeshClass, Selected, Context).IsSet();
	}

	bool IsDetachOffered(UClass* MeshClass, const FAssetData& Selected)
	{
		FToolMenuContext Context;
		return OfferedDetachEntryForAssets(MeshClass, TConstArrayView<FAssetData>(&Selected, 1), Context).IsSet();
	}

	/** The asset registry's record of a saved asset read from its package file, with its package unloaded, as the Content Browser lists it. */
	FAssetData UnloadedAssetData(UObject* Asset)
	{
		const FSoftObjectPath Path(Asset);
		const FString File = PackageFileOf(Asset);
		UnloadPackage(Asset->GetPackage());
		IAssetRegistry::GetChecked().ScanFilesSynchronous({ File }, true);
		return IAssetRegistry::GetChecked().GetAssetByObjectPath(Path);
	}

	/** Runs Detach from Grammar from the right-click menu of MeshClass on the selection; returns whether it was offered and ran. */
	bool RunDetachEntry(UClass* MeshClass, TConstArrayView<UObject*> Selected)
	{
		FToolMenuContext Context;
		TOptional<FToolMenuEntry> Entry = OfferedDetachEntry(MeshClass, Selected, Context);
		return Entry && Entry->TryExecuteToolUIAction(Context);
	}

	bool ListsOutput(const UCambrianaGrammar& Grammar, const UObject* Mesh)
	{
		return UCambrianaAssetImportData::FindMeshesLinkedTo(Grammar).ContainsByPredicate(
			[Mesh](const FAssetData& Asset) { return Asset.GetSoftObjectPath() == FSoftObjectPath(Mesh); });
	}

	/** SDD-unreal.md, Detach: plain UAssetImportData with no source file, as the Source File row's Clear leaves it. */
	bool IsDetached(const UObject* Mesh)
	{
		const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh);
		const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh);
		const UAssetImportData* ImportData = StaticMesh ? StaticMesh->GetAssetImportData() : SkeletalMesh ? SkeletalMesh->GetAssetImportData() : nullptr;
		return ImportData && ImportData->GetClass() == UAssetImportData::StaticClass() && ImportData->GetSourceFileCount() == 0;
	}

	bool LinksTo(const UObject* Mesh, const UCambrianaGrammar* Grammar)
	{
		const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
		return ImportData && ImportData->Grammar.ToSoftObjectPath() == FSoftObjectPath(Grammar);
	}

	template <typename MeshType>
	void SetFbxImportData(MeshType& Mesh)
	{
		UAssetImportData* FbxData = NewObject<UAssetImportData>(&Mesh);
		FbxData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("tree.fbx")));
		Mesh.SetAssetImportData(FbxData);
	}

	UStaticMesh* MeshWithFbxImportData()
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
		SetFbxImportData(*Mesh);
		return Mesh;
	}
}

// ATS-44.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaDetachAts44Test,
	"Cambriana.Editor.Detach.ADetachedMeshIsUnchangedByReimportOfTheLinkedMeshHasReimportDisabledAndIsNotAmongTheGrammarsOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaDetachAts44Test::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("DetachAts"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Detached = Fixture.FindMesh();
	USkeletalMesh* Linked = Fixture.FindSkeletalMesh();
	TestTrue(TEXT("Both baked meshes are the grammar's outputs before Detach"), ListsOutput(*Fixture.Grammar, Detached) && ListsOutput(*Fixture.Grammar, Linked));
	if (!TestTrue(TEXT("Detach from Grammar runs from the static mesh's right-click menu"), RunDetachEntry(UStaticMesh::StaticClass(), { Detached })))
	{
		return false;
	}
	if (!ChangeLengthAndSave(*this, *Fixture.Grammar, 50.0))
	{
		return false;
	}

	TestTrue(TEXT("Reimport runs on the linked skeletal mesh"), Reimport(Linked));

	TestTrue(TEXT("The detached mesh is the same object"), Fixture.FindMesh() == Detached);
	TestEqual(TEXT("The detached mesh is unchanged: 20 cm tall, from len 100"), MeshHeight(Detached), HeightAtFullLength, VertexTolerance);
	TestTrue(TEXT("Its import data is plain import data with no source file"), IsDetached(Detached));
	TestTrue(TEXT("The Content Browser reads no source file for it, so its Reimport is disabled"), ContentBrowserSourceFiles(Detached).IsEmpty());
	TestFalse(TEXT("The engine's reimport manager cannot reimport it"), FReimportManager::Instance()->CanReimport(Detached));
	TestFalse(TEXT("Reimport on it does not run"), Reimport(Detached));
	TestEqual(TEXT("It is still 20 cm tall"), MeshHeight(Detached), HeightAtFullLength, VertexTolerance);
	TestFalse(TEXT("It is not among the grammar's outputs"), ListsOutput(*Fixture.Grammar, Detached));
	TestTrue(TEXT("The linked mesh is among the grammar's outputs"), ListsOutput(*Fixture.Grammar, Linked));
	TestTrue(TEXT("The linked mesh keeps its link"), LinksTo(Linked, Fixture.Grammar));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaDetachUndoTest,
	"Cambriana.Editor.Detach.DetachIsOneUndoableChangeThatMarksTheMeshDirtyAndUndoRestoresTheLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaDetachUndoTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("DetachUndo"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	USkeletalMesh* Mesh = Fixture.FindSkeletalMesh();
	const UAssetImportData* Link = Mesh->GetAssetImportData();
	const FString GrammarFile = PackageFilename(*Fixture.GrammarPackage);
	TestFalse(TEXT("The baked mesh is saved"), Mesh->GetPackage()->IsDirty());
	const int32 HistoryBefore = UndoHistoryLength();

	if (!TestTrue(TEXT("Detach from Grammar runs from the skeletal mesh's right-click menu"), RunDetachEntry(USkeletalMesh::StaticClass(), { Mesh })))
	{
		return false;
	}
	TestTrue(TEXT("The mesh is detached"), IsDetached(Mesh));
	TestTrue(TEXT("Its package is marked dirty"), Mesh->GetPackage()->IsDirty());
	TestEqual(TEXT("Detach adds one step to the undo history"), UndoHistoryLength(), HistoryBefore + 1);

	TestTrue(TEXT("Detach can be undone"), GEditor->UndoTransaction());
	TestTrue(TEXT("Undo restores the mesh's link, the same import data"), Mesh->GetAssetImportData() == Link && LinksTo(Mesh, Fixture.Grammar));
	const TArray<FString> BrowserFiles = ContentBrowserSourceFiles(Mesh);
	TestTrue(TEXT("The Content Browser reads the grammar's package file again, so Reimport is enabled"), BrowserFiles.Num() == 1 && SameFile(BrowserFiles[0], GrammarFile));
	TestTrue(TEXT("The mesh is among the grammar's outputs again"), ListsOutput(*Fixture.Grammar, Mesh));

	TestTrue(TEXT("Detach can be redone"), GEditor->RedoTransaction());
	TestTrue(TEXT("Redo detaches the mesh again"), IsDetached(Mesh));
	TestFalse(TEXT("It is not among the grammar's outputs"), ListsOutput(*Fixture.Grammar, Mesh));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaDetachOfferedTest,
	"Cambriana.Editor.Detach.DetachFromGrammarIsOfferedInTheAssetActionsOfMeshesLinkedToAGrammarOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaDetachOfferedTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("DetachOffered"));
	if (!TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	FToolMenuContext Context;
	const TOptional<FToolMenuEntry> StaticEntry = OfferedDetachEntry(UStaticMesh::StaticClass(), { Fixture.FindMesh() }, Context);
	if (TestTrue(TEXT("A baked static mesh's asset actions offer Detach from Grammar"), StaticEntry.IsSet()))
	{
		TestEqual(TEXT("It is labelled Detach from Grammar"), StaticEntry->Label.Get().ToString(), FString(TEXT("Detach from Grammar")));
	}
	TestTrue(TEXT("A baked skeletal mesh's asset actions offer it"), IsDetachOffered(USkeletalMesh::StaticClass(), { Fixture.FindSkeletalMesh() }));

	TestFalse(TEXT("A static mesh without import data is not offered it"), IsDetachOffered(UStaticMesh::StaticClass(), { NewObject<UStaticMesh>(GetTransientPackage()) }));
	TestFalse(TEXT("A static mesh imported from an FBX file is not offered it"), IsDetachOffered(UStaticMesh::StaticClass(), { MeshWithFbxImportData() }));
	USkeletalMesh* PlainSkeletal = NewObject<USkeletalMesh>(GetTransientPackage());
	PlainSkeletal->SetAssetImportData(NewObject<UAssetImportData>(PlainSkeletal));
	TestFalse(TEXT("A skeletal mesh with plain import data is not offered it"), IsDetachOffered(USkeletalMesh::StaticClass(), { PlainSkeletal }));

	if (TestTrue(TEXT("Detach from Grammar runs on the static mesh"), RunDetachEntry(UStaticMesh::StaticClass(), { Fixture.FindMesh() })))
	{
		TestFalse(TEXT("Once detached, the static mesh is not offered it"), IsDetachOffered(UStaticMesh::StaticClass(), { Fixture.FindMesh() }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaDetachOfferedWithoutLoadingTest,
	"Cambriana.Editor.Detach.DetachFromGrammarIsOfferedOnAnUnloadedLinkedMeshAndNotOnAnUnloadedFbxMeshWithoutLoadingEither",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaDetachOfferedWithoutLoadingTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("DetachNoLoad"));
	FBakeFixture Imported(TEXT("DetachNoLoadFbx"));
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The other static mesh bakes"), Imported.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* FromFbx = Imported.FindMesh();
	SetFbxImportData(*FromFbx);
	if (!TestTrue(TEXT("The mesh imported from an FBX file saves"), Save(FromFbx)))
	{
		return false;
	}
	const FAssetData Linked = UnloadedAssetData(Fixture.FindMesh());
	const FAssetData Fbx = UnloadedAssetData(FromFbx);
	if (!TestTrue(TEXT("The registry lists both meshes"), Linked.IsValid() && Fbx.IsValid())
		|| !TestTrue(TEXT("Neither is loaded"), !FindPackage(nullptr, *Fixture.MeshPackage) && !FindPackage(nullptr, *Imported.MeshPackage)))
	{
		return false;
	}

	// ContentBrowserMenuContexts.h: the Content Browser no longer loads assets on right click; a menu decides from SelectedAssets.
	TestTrue(TEXT("A linked mesh's asset actions offer Detach from Grammar"), IsDetachOffered(UStaticMesh::StaticClass(), Linked));
	TestFalse(TEXT("A mesh imported from an FBX file is not offered it"), IsDetachOffered(UStaticMesh::StaticClass(), Fbx));
	TestNull(TEXT("The linked mesh is not loaded to decide"), FindPackage(nullptr, *Fixture.MeshPackage));
	TestNull(TEXT("The FBX mesh is not loaded to decide"), FindPackage(nullptr, *Imported.MeshPackage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaDetachSelectionTest,
	"Cambriana.Editor.Detach.DetachFromGrammarDetachesEverySelectedLinkedMeshInOneUndoableChangeAndLeavesOthersAlone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaDetachSelectionTest::RunTest(const FString& Parameters)
{
	FBakeFixture First(TEXT("DetachSelectFirst"));
	FBakeFixture Second(TEXT("DetachSelectSecond"));
	if (!TestTrue(TEXT("The first static mesh bakes"), First.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The second static mesh bakes"), Second.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* FromFbx = MeshWithFbxImportData();
	const UAssetImportData* FbxData = FromFbx->GetAssetImportData();
	const int32 HistoryBefore = UndoHistoryLength();

	if (!TestTrue(TEXT("Detach from Grammar is offered and runs on the selection"), RunDetachEntry(UStaticMesh::StaticClass(), { First.FindMesh(), FromFbx, Second.FindMesh() })))
	{
		return false;
	}
	TestTrue(TEXT("The first linked mesh is detached"), IsDetached(First.FindMesh()));
	TestTrue(TEXT("The second linked mesh is detached"), IsDetached(Second.FindMesh()));
	TestTrue(TEXT("Both packages are marked dirty"), First.FindMesh()->GetPackage()->IsDirty() && Second.FindMesh()->GetPackage()->IsDirty());
	TestTrue(TEXT("The FBX mesh keeps its import data"), FromFbx->GetAssetImportData() == FbxData && FbxData->GetSourceFileCount() == 1);
	TestEqual(TEXT("Detach adds one step to the undo history"), UndoHistoryLength(), HistoryBefore + 1);

	TestTrue(TEXT("Detach can be undone"), GEditor->UndoTransaction());
	TestTrue(TEXT("One Undo restores both links"), LinksTo(First.FindMesh(), First.Grammar) && LinksTo(Second.FindMesh(), Second.Grammar));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarOutputsTest,
	"Cambriana.Editor.Detach.TheGrammarsOutputsAreTheMeshesLinkedToItAndNoOthers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarOutputsTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("OutputsFirst"));
	FBakeFixture Other(TEXT("OutputsOther"));
	if (!TestTrue(TEXT("The grammar saves"), Save(Fixture.Grammar)) || !TestTrue(TEXT("The other grammar saves"), Save(Other.Grammar))
		|| !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The skeletal mesh bakes"), Fixture.BakeSkeletal().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The other grammar's static mesh bakes"), Other.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	const TArray<FAssetData> Outputs = UCambrianaAssetImportData::FindMeshesLinkedTo(*Fixture.Grammar);
	TestEqual(TEXT("The grammar has two outputs"), Outputs.Num(), 2);
	TestTrue(TEXT("Its static mesh is one"), ListsOutput(*Fixture.Grammar, Fixture.FindMesh()));
	TestTrue(TEXT("Its skeletal mesh is the other"), ListsOutput(*Fixture.Grammar, Fixture.FindSkeletalMesh()));
	TestFalse(TEXT("The other grammar's static mesh is not an output"), ListsOutput(*Fixture.Grammar, Other.FindMesh()));
	const TArray<FAssetData> OtherOutputs = UCambrianaAssetImportData::FindMeshesLinkedTo(*Other.Grammar);
	TestTrue(TEXT("The other grammar's only output is its static mesh"), OtherOutputs.Num() == 1 && ListsOutput(*Other.Grammar, Other.FindMesh()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarOutputsWithoutLoadingTest,
	"Cambriana.Editor.Detach.TheGrammarsOutputsAreFoundWithoutLoadingAMeshThatReferencesTheGrammarButIsImportedFromAnFbxFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarOutputsWithoutLoadingTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("OutputsNoLoad"));
	FBakeFixture Other(TEXT("OutputsNoLoadFbx"));
	if (!TestTrue(TEXT("The grammar saves"), Save(Fixture.Grammar))
		|| !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked)
		|| !TestTrue(TEXT("The other skeletal mesh bakes"), Other.BakeSkeletal().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	// A skeletal mesh imported from an FBX file whose package references the grammar's, through its soft Default Animating Rig.
	USkeletalMesh* Referencing = Other.FindSkeletalMesh();
	SetFbxImportData(*Referencing);
	Referencing->SetDefaultAnimatingRig(TSoftObjectPtr<UObject>(FSoftObjectPath(Fixture.Grammar)));
	if (!TestTrue(TEXT("The referencing mesh saves"), Save(Referencing)))
	{
		return false;
	}
	UnloadedAssetData(Referencing);
	TArray<FName> Referencers;
	IAssetRegistry::GetChecked().GetReferencers(FName(*Fixture.GrammarPackage), Referencers);
	if (!TestTrue(TEXT("The registry lists the FBX mesh's package among the grammar's referencers"), Referencers.Contains(FName(*Other.SkeletalMeshPackage)))
		|| !TestNull(TEXT("The FBX mesh is not loaded"), FindPackage(nullptr, *Other.SkeletalMeshPackage)))
	{
		return false;
	}

	const TArray<FAssetData> Outputs = UCambrianaAssetImportData::FindMeshesLinkedTo(*Fixture.Grammar);
	TestTrue(TEXT("The grammar's only output is its static mesh"), Outputs.Num() == 1 && ListsOutput(*Fixture.Grammar, Fixture.FindMesh()));
	TestNull(TEXT("The FBX mesh is not loaded to decide"), FindPackage(nullptr, *Other.SkeletalMeshPackage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarOutputsMovedTest,
	"Cambriana.Editor.Detach.AMovedGrammarsOutputsAreTheMeshesThatReferenceItOnceTheyAreSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarOutputsMovedTest::RunTest(const FString& Parameters)
{
	FBakeFixture Fixture(TEXT("OutputsMoved"));
	const FString MovedPath = FString(BakeTestRoot) / TEXT("OutputsMovedTo");
	const FString MovedPackage = MovedPath / TEXT("OutputsMovedRenamed");
	const FString MovedFile = PackageFilename(*MovedPackage);
	ON_SCOPE_EXIT
	{
		if (UPackage* Package = FindPackage(nullptr, *MovedPackage))
		{
			UnloadPackage(Package);
		}
		IFileManager::Get().Delete(*MovedFile);
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(MovedFile), false, true);
	};
	if (!PrepareShapeGrammar(*this, Fixture) || !TestTrue(TEXT("The static mesh bakes"), Fixture.BakeStatic().Status == ECambrianaBakeStatus::Baked))
	{
		return false;
	}
	UStaticMesh* Mesh = Fixture.FindMesh();
	UCambrianaGrammar* Grammar = Fixture.Grammar;
	// As the moved grammar Reimport test: the test does not tick, so the asset registry reads the saved mesh's dependencies now.
	IAssetRegistry::GetChecked().ScanFilesSynchronous({ PackageFileOf(Mesh) }, true);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	if (!TestTrue(TEXT("The grammar is moved and renamed"), AssetTools.RenameAssets({ FAssetRenameData(TWeakObjectPtr<UObject>(Grammar), MovedPath, FString(TEXT("OutputsMovedRenamed"))) })))
	{
		return false;
	}
	const UCambrianaAssetImportData* ImportData = UCambrianaAssetImportData::Of(Mesh);
	TestTrue(TEXT("The mesh's source file still names the grammar's old package file"), ImportData && !SameFile(ImportData->GetFirstFilename(), MovedFile));
	if (!TestTrue(TEXT("The mesh saves"), Save(Mesh)))
	{
		return false;
	}
	IAssetRegistry::GetChecked().ScanFilesSynchronous({ PackageFileOf(Mesh) }, true);

	TestTrue(TEXT("The mesh is among the moved grammar's outputs"), ListsOutput(*Grammar, Mesh));
	return true;
}
#endif
