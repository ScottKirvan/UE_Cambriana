#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaTestUtilities.h"
#include "CambrianaTestWorld.h"
#include "Components/DynamicMeshComponent.h"
#include "Misc/AutomationTest.h"

#include <cambriana/cambriana.h>

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;
using Cambriana::Tests::LoadFixture;

namespace
{
	constexpr uint32 TestSeed = 12345;

	struct FCoreCounts
	{
		int32 Vertices = -1;
		int32 Triangles = -1;
	};

	FCoreCounts LegacyCoreCounts(const FString& Text, uint64 Sides)
	{
		cambriana_build_settings Settings;
		cambriana_build_settings_init(&Settings, sizeof(Settings));
		Settings.seed = TestSeed;
		Settings.sides = Sides;
		const FTCHARToUTF8 Utf8(*Text);
		cambriana_result* Result = nullptr;
		cambriana_build_legacy(Utf8.Get(), Utf8.Length(), &Settings, nullptr, &Result);
		FCoreCounts Counts;
		if (Result)
		{
			uint64_t Count = 0;
			cambriana_result_surface_positions(Result, &Count);
			Counts.Vertices = static_cast<int32>(Count);
			cambriana_result_triangles(Result, &Count);
			Counts.Triangles = static_cast<int32>(Count);
			cambriana_result_destroy(Result);
		}
		return Counts;
	}

	UCambrianaGrammar* MakeGrammar(const FString& Text)
	{
		UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
		Grammar->GrammarText = Text;
		return Grammar;
	}

	ACambrianaLiveActor* SpawnActor(UWorld* World)
	{
		ACambrianaLiveActor* Actor = World->SpawnActor<ACambrianaLiveActor>();
		if (Actor)
		{
			Actor->Seed = TestSeed;
		}
		return Actor;
	}

	const FDynamicMesh3& MeshOf(const ACambrianaLiveActor* Actor)
	{
		return *Actor->GetDynamicMeshComponent()->GetMesh();
	}

	TArray<FVector3d> PositionsOf(const ACambrianaLiveActor* Actor)
	{
		TArray<FVector3d> Positions;
		const FDynamicMesh3& Mesh = MeshOf(Actor);
		for (const int32 Vid : Mesh.VertexIndicesItr())
		{
			Positions.Add(Mesh.GetVertex(Vid));
		}
		return Positions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSettingsDefaultsTest,
	"Cambriana.GrammarAsset.Settings.DefaultsMatchTheLiveActorDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UCambrianaGrammar* Grammar = NewObject<UCambrianaGrammar>(GetTransientPackage());
	const ACambrianaLiveActor* Actor = GetDefault<ACambrianaLiveActor>();
	TestTrue(TEXT("Size mode"), Grammar->SizeMode == Actor->SizeMode);
	TestEqual(TEXT("Size value"), Grammar->SizeValue, Actor->SizeValue);
	TestEqual(TEXT("Closed form"), Grammar->bClosedForm, Actor->bClosedForm);
	TestEqual(TEXT("Sides"), Grammar->Sides, Actor->Sides);
	TestEqual(TEXT("Min thickness"), Grammar->MinThickness, Actor->MinThickness);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSettingsEditableTest,
	"Cambriana.GrammarAsset.Settings.AreEditableOnTheAsset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSettingsEditableTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("SizeMode"), TEXT("SizeValue"), TEXT("bClosedForm"), TEXT("Sides"), TEXT("MinThickness") })
	{
		const FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(Name);
		if (TestNotNull(FString::Printf(TEXT("%s is a property"), Name), Property))
		{
			TestTrue(FString::Printf(TEXT("%s is editable"), Name), Property->HasAnyPropertyFlags(CPF_Edit));
			TestFalse(FString::Printf(TEXT("%s is not read-only"), Name), Property->HasAnyPropertyFlags(CPF_EditConst));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSidesTest,
	"Cambriana.GrammarAsset.Settings.AssetSidesShapeAReferencingActorsTubes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSidesTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.ls"));
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnActor(World.Get());
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Text.IsEmpty()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(Text);
	Grammar->Sides = 3;
	TestEqual(TEXT("The actor's own sides differ from the asset's"), Actor->Sides, 8);
	Actor->SetGrammar(Grammar);

	const FCoreCounts Core = LegacyCoreCounts(Text, 3);
	TestEqual(TEXT("The core builds 84 rings of 3 vertices"), Core.Vertices, 84 * 3);
	TestEqual(TEXT("Vertex count matches the core's at 3 sides"), MeshOf(Actor).VertexCount(), Core.Vertices);
	TestEqual(TEXT("Triangle count matches the core's at 3 sides"), MeshOf(Actor).TriangleCount(), Core.Triangles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSurfacerAndSizeTest,
	"Cambriana.GrammarAsset.Settings.AReferencingActorBuildsLikeAnInlineActorWithTheAssetsSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSurfacerAndSizeTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.ls"));
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Text.IsEmpty()))
	{
		return false;
	}

	FCambrianaTestWorld World;
	ACambrianaLiveActor* Inline = SpawnActor(World.Get());
	ACambrianaLiveActor* FromAsset = SpawnActor(World.Get());
	if (!TestNotNull(TEXT("Inline actor spawned"), Inline) || !TestNotNull(TEXT("Asset actor spawned"), FromAsset))
	{
		return false;
	}
	Inline->InlineGrammar = Text;
	Inline->SizeMode = ECambrianaSizeMode::Step;
	Inline->SizeValue = 10.0;
	Inline->bClosedForm = false;
	Inline->MinThickness = 20.0;
	Inline->Rebuild();

	UCambrianaGrammar* Grammar = MakeGrammar(Text);
	Grammar->SizeMode = ECambrianaSizeMode::Step;
	Grammar->SizeValue = 10.0;
	Grammar->bClosedForm = false;
	Grammar->MinThickness = 20.0;
	FromAsset->SetGrammar(Grammar);

	const FCambrianaBuildReport& Report = FromAsset->GetLastBuildReport();
	TestTrue(TEXT("The asset's size mode is used"), Report.SizeMode == ECambrianaSizeMode::Step);
	TestNearlyEqual(TEXT("The asset's step length is used"), Report.StepLength, 10.0, 1e-9);
	TestTrue(TEXT("Built geometry"), MeshOf(FromAsset).TriangleCount() > 0);
	TestEqual(TEXT("Vertex count matches the inline actor's"), MeshOf(FromAsset).VertexCount(), MeshOf(Inline).VertexCount());
	TestTrue(TEXT("Vertex positions match the inline actor's"), PositionsOf(FromAsset) == PositionsOf(Inline));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSizeForLsysTest,
	"Cambriana.GrammarAsset.Settings.AssetSizeAppliesToLsysTextStatingNoSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSizeForLsysTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnActor(World.Get());
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(Text);
	Grammar->SizeMode = ECambrianaSizeMode::Fit;
	Grammar->SizeValue = 75.0;
	Actor->SetGrammar(Grammar);

	const FCambrianaBuildReport& Report = Actor->GetLastBuildReport();
	TestFalse(TEXT("No errors"), Report.bHasErrors);
	TestTrue(TEXT("Fit mode is used"), Report.SizeMode == ECambrianaSizeMode::Fit);
	TestNearlyEqual(TEXT("The asset's fit size is used"), Report.SizeValue, 75.0, 1e-9);
	TestNearlyEqual(TEXT("The actor shows the size used"), Actor->SizeValue, 75.0, 1e-9);
	const FVector3d Size = MeshOf(Actor).GetBounds().Diagonal();
	TestNearlyEqual(TEXT("The largest dimension is the asset's fit size"), FMath::Max3(Size.X, Size.Y, Size.Z), 75.0, 1e-3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetLsysSizeWinsTest,
	"Cambriana.GrammarAsset.Settings.LsysTextSizeWinsOverAssetSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetLsysSizeWinsTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.lsys"));
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnActor(World.Get());
	if (!TestFalse(TEXT("sk.lsys fixture loaded"), Text.IsEmpty()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(TEXT("size 50cm\n") + Text);
	Grammar->SizeMode = ECambrianaSizeMode::Step;
	Grammar->SizeValue = 3.0;
	Actor->SetGrammar(Grammar);

	const FCambrianaBuildReport& Report = Actor->GetLastBuildReport();
	TestFalse(TEXT("No errors"), Report.bHasErrors);
	TestTrue(TEXT("The text's fit mode is used"), Report.SizeMode == ECambrianaSizeMode::Fit);
	TestNearlyEqual(TEXT("The text's size is used"), Report.SizeValue, 50.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetActorSizeOverrideTest,
	"Cambriana.GrammarAsset.Settings.ActorSizeOverrideWinsOverAssetSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetActorSizeOverrideTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Fixture : { TEXT("sk.ls"), TEXT("sk.lsys") })
	{
		const FString Text = LoadFixture(Fixture);
		FCambrianaTestWorld World;
		ACambrianaLiveActor* Actor = SpawnActor(World.Get());
		if (!TestFalse(FString::Printf(TEXT("%s fixture loaded"), Fixture), Text.IsEmpty()) || !TestNotNull(TEXT("Actor spawned"), Actor))
		{
			continue;
		}
		UCambrianaGrammar* Grammar = MakeGrammar(Text);
		Grammar->SizeMode = ECambrianaSizeMode::Step;
		Grammar->SizeValue = 10.0;
		Actor->bOverrideSize = true;
		Actor->SizeMode = ECambrianaSizeMode::Fit;
		Actor->SizeValue = 300.0;
		Actor->SetGrammar(Grammar);

		const FCambrianaBuildReport& Report = Actor->GetLastBuildReport();
		TestTrue(FString::Printf(TEXT("%s: the actor's size mode is used"), Fixture), Report.SizeMode == ECambrianaSizeMode::Fit);
		TestNearlyEqual(FString::Printf(TEXT("%s: the actor's fit size is used"), Fixture), Report.SizeValue, 300.0, 1e-9);

		Actor->bOverrideSize = false;
		Actor->Rebuild();
		TestTrue(FString::Printf(TEXT("%s: with the override off the asset's size mode is used"), Fixture), Actor->GetLastBuildReport().SizeMode == ECambrianaSizeMode::Step);
		TestNearlyEqual(FString::Printf(TEXT("%s: with the override off the asset's step is used"), Fixture), Actor->GetLastBuildReport().StepLength, 10.0, 1e-9);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSurfacerOverridesTest,
	"Cambriana.GrammarAsset.Settings.ActorSurfacerOverridesWinOverAssetSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSurfacerOverridesTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.ls"));
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnActor(World.Get());
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Text.IsEmpty()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	TestFalse(TEXT("Sides override is off by default"), Actor->bOverrideSides);
	TestFalse(TEXT("Closed form override is off by default"), Actor->bOverrideClosedForm);
	TestFalse(TEXT("Min thickness override is off by default"), Actor->bOverrideMinThickness);

	UCambrianaGrammar* Grammar = MakeGrammar(Text);
	Grammar->Sides = 3;
	Grammar->bClosedForm = true;
	Grammar->MinThickness = 0.0;
	Actor->bOverrideSides = true;
	Actor->Sides = 5;
	Actor->bOverrideClosedForm = true;
	Actor->bClosedForm = false;
	Actor->bOverrideMinThickness = true;
	Actor->MinThickness = 20.0;
	Actor->SetGrammar(Grammar);

	const FCambrianaBuildSettings& Used = Actor->GetLastBuildInput().Settings;
	TestEqual(TEXT("The actor's sides are used"), Used.Sides, 5);
	TestFalse(TEXT("The actor's closed form is used"), Used.bClosedForm);
	TestEqual(TEXT("The actor's min thickness is used"), Used.MinThickness, 20.0);

	Actor->bOverrideClosedForm = false;
	Actor->bOverrideMinThickness = false;
	Actor->Rebuild();
	TestEqual(TEXT("Closed tubes with the actor's sides: 84 rings of 5 vertices"), MeshOf(Actor).VertexCount(), 84 * 5);

	Actor->bOverrideSides = false;
	Actor->Rebuild();
	TestEqual(TEXT("With every override off the asset's sides are used"), MeshOf(Actor).VertexCount(), LegacyCoreCounts(Text, 3).Vertices);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetShownSurfacerTest,
	"Cambriana.GrammarAsset.Settings.ActorShowsTheAssetsSurfacerSettingsAndOverridesTurnedOnChangeNothing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetShownSurfacerTest::RunTest(const FString& Parameters)
{
	const FString Text = LoadFixture(TEXT("sk.ls"));
	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnActor(World.Get());
	if (!TestFalse(TEXT("sk.ls fixture loaded"), Text.IsEmpty()) || !TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(Text);
	Grammar->SizeMode = ECambrianaSizeMode::Step;
	Grammar->SizeValue = 10.0;
	Grammar->Sides = 3;
	Grammar->bClosedForm = false;
	Grammar->MinThickness = 20.0;
	Actor->SetGrammar(Grammar);

	TestEqual(TEXT("Shown sides are the asset's"), Actor->Sides, 3);
	TestFalse(TEXT("Shown closed form is the asset's"), Actor->bClosedForm);
	TestEqual(TEXT("Shown min thickness is the asset's"), Actor->MinThickness, 20.0);

	const TArray<FVector3d> Baseline = PositionsOf(Actor);
	TestTrue(TEXT("Built geometry"), Baseline.Num() > 0);
	struct FOverride
	{
		const TCHAR* Name;
		bool ACambrianaLiveActor::*Flag;
	};
	const FOverride Overrides[] = {
		{ TEXT("sides"), &ACambrianaLiveActor::bOverrideSides },
		{ TEXT("closed form"), &ACambrianaLiveActor::bOverrideClosedForm },
		{ TEXT("min thickness"), &ACambrianaLiveActor::bOverrideMinThickness },
		{ TEXT("size"), &ACambrianaLiveActor::bOverrideSize },
	};
	for (const FOverride& Override : Overrides)
	{
		Actor->*Override.Flag = true;
		Actor->Rebuild();
		TestTrue(FString::Printf(TEXT("Turning the %s override on leaves the vertex positions unchanged"), Override.Name), PositionsOf(Actor) == Baseline);
		Actor->*Override.Flag = false;
		Actor->Rebuild();
	}
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCambrianaGrammarAssetSettingChangeRebuildsTest,
	"Cambriana.GrammarAsset.Settings.ChangingAnAssetSettingRebuildsReferencingActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCambrianaGrammarAssetSettingChangeRebuildsTest::RunTest(const FString& Parameters)
{
	using Cambriana::Tests::EditProperty;

	FCambrianaTestWorld World;
	ACambrianaLiveActor* Actor = SpawnActor(World.Get());
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}
	UCambrianaGrammar* Grammar = MakeGrammar(LoadFixture(TEXT("sk.ls")));
	Actor->SetGrammar(Grammar);

	const auto ExpectRebuild = [this, Actor, Grammar](FName Property, TFunctionRef<void()> Change, TFunctionRef<bool(const FCambrianaBuildSettings&)> Used)
	{
		const int32 Before = Actor->GetBuildCount();
		EditProperty(Grammar, Property, Change);
		TestEqual(FString::Printf(TEXT("Changing the asset's %s rebuilds the actor once"), *Property.ToString()), Actor->GetBuildCount(), Before + 1);
		TestTrue(FString::Printf(TEXT("The rebuild used the asset's new %s"), *Property.ToString()), Used(Actor->GetLastBuildInput().Settings));
	};
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, Sides), [Grammar] { Grammar->Sides = 5; },
		[](const FCambrianaBuildSettings& Settings) { return Settings.Sides == 5; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, bClosedForm), [Grammar] { Grammar->bClosedForm = false; },
		[](const FCambrianaBuildSettings& Settings) { return !Settings.bClosedForm; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, MinThickness), [Grammar] { Grammar->MinThickness = 2.0; },
		[](const FCambrianaBuildSettings& Settings) { return Settings.MinThickness == 2.0; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, SizeMode), [Grammar] { Grammar->SizeMode = ECambrianaSizeMode::Step; },
		[](const FCambrianaBuildSettings& Settings) { return Settings.SizeMode == ECambrianaSizeMode::Step; });
	ExpectRebuild(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, SizeValue), [Grammar] { Grammar->SizeValue = 4.0; },
		[](const FCambrianaBuildSettings& Settings) { return Settings.SizeValue == 4.0; });
	return true;
}
#endif

#endif
