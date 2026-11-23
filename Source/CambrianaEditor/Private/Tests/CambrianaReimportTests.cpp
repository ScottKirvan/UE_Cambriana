#include "Animation/SkeletalMeshActor.h"
#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "AssetToolsModule.h"
#include "CambrianaAssetImportData.h"
#include "CambrianaBakeTestFixture.h"
#include "CambrianaEditorTestUtilities.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaMeshComparison.h"
#include "CambrianaReimportFactory.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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
	 * asset no handler claims is not reimported, since the engine would ask for a file in a dialog.
	 */
	bool Reimport(UObject* Asset)
	{
		return FReimportManager::Instance()->CanReimport(Asset)
			&& FReimportManager::Instance()->Reimport(Asset, false, false, FString(), nullptr, INDEX_NONE, false, true);
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
	TestTrue(TEXT("Reimport succeeds without asking to overwrite"), Reimport(Mesh));

	TestTrue(TEXT("The same static mesh object is rebuilt"), Fixture.FindMesh() == Mesh);
	TestEqual(TEXT("It is rebuilt from the changed len of 50: 15 cm tall"), MeshHeight(Mesh), HeightAtHalfLength, VertexTolerance);
	TestFalse(TEXT("It is saved, as Bake saves"), Mesh->GetPackage()->IsDirty());
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

	TestTrue(TEXT("Reimport succeeds without asking to overwrite"), Reimport(Mesh));

	TestTrue(TEXT("The same skeletal mesh object is rebuilt"), Fixture.FindSkeletalMesh() == Mesh);
	TestTrue(TEXT("It keeps its skeleton, the same object"), Mesh->GetSkeleton() == Skeleton && Fixture.FindSkeleton() == Skeleton);
	TestTrue(TEXT("It keeps its physics asset, the same object"), Mesh->GetPhysicsAsset() == PhysicsAsset && Fixture.FindPhysicsAsset() == PhysicsAsset);
	TestFalse(TEXT("The skeletal mesh, skeleton and physics asset are saved, as Bake saves"),
		Mesh->GetPackage()->IsDirty() || Skeleton->GetPackage()->IsDirty() || PhysicsAsset->GetPackage()->IsDirty());
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

#endif
