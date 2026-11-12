#include "CambrianaLiveActor.h"

#include "CambrianaGrammar.h"
#include "CambrianaPreviewMaterials.h"
#include "CambrianaSeed.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

ACambrianaLiveActor::ACambrianaLiveActor()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(Cambriana::PreviewBaseMaterialPath);
	PreviewBaseMaterial = BaseMaterial.Object;
}

void ACambrianaLiveActor::SetGrammar(UCambrianaGrammar* NewGrammar)
{
	Grammar = NewGrammar;
	Rebuild();
}

void ACambrianaLiveActor::PlaceFromGrammar(UCambrianaGrammar* Source)
{
	if (Source)
	{
		Seed = Source->Seed;
	}
	SetGrammar(Source);
}

FCambrianaBuildInput ACambrianaLiveActor::MakeBuildInput() const
{
	return MakeBuildInputWith(Grammar, Seed);
}

FCambrianaBuildInput ACambrianaLiveActor::MakeBuildInputFor(const UCambrianaGrammar& Grammar)
{
	return GetDefault<ACambrianaLiveActor>()->MakeBuildInputWith(&Grammar, Grammar.Seed);
}

FCambrianaBuildInput ACambrianaLiveActor::MakeBuildInputWith(const UCambrianaGrammar* InGrammar, uint32 InSeed) const
{
	FCambrianaBuildInput Input;
	Input.GrammarText = InGrammar ? InGrammar->GrammarText : InlineGrammar;
	Input.Seed = InSeed;

	FCambrianaBuildSettings& Settings = Input.Settings;
	if (bOverrideRecursion)
	{
		Settings.Recursion = Recursion;
	}
	if (bOverrideBasicAngle)
	{
		Settings.BasicAngle = BasicAngle;
	}
	if (bOverrideThickness)
	{
		Settings.Thickness = Thickness;
	}
	Settings.MinThickness = InGrammar && !bOverrideMinThickness ? InGrammar->MinThickness : MinThickness;
	Settings.bClosedForm = InGrammar && !bOverrideClosedForm ? InGrammar->bClosedForm : bClosedForm;
	Settings.Sides = InGrammar && !bOverrideSides ? InGrammar->Sides : Sides;
	Settings.bOverrideGrammarMinThickness = bOverrideMinThickness;
	Settings.bOverrideGrammarSurface = bOverrideClosedForm;
	Settings.bOverrideGrammarSides = bOverrideSides;
	const bool bAssetSize = InGrammar && !bOverrideSize;
	Settings.SizeMode = bAssetSize ? InGrammar->SizeMode : SizeMode;
	Settings.SizeValue = bAssetSize ? InGrammar->SizeValue : SizeValue;
	Settings.bOverrideGrammarSize = bOverrideSize;
	Settings.bSizeForGrammarWithoutSize = bAssetSize;
	return Input;
}

void ACambrianaLiveActor::Rebuild()
{
	LastBuildInput = MakeBuildInput();
	FCambrianaBuildResult Result = Cambriana::BuildMesh(LastBuildInput);
	LastBuildReport = MoveTemp(Result.Report);
	ShowSettingsUsed();
	if (UDynamicMeshComponent* MeshComponent = GetDynamicMeshComponent())
	{
		MeshComponent->SetMesh(MoveTemp(Result.Mesh));
		Cambriana::ApplyPreviewMaterials(*MeshComponent, Result.SlotMaterialIds, PreviewBaseMaterial, PreviewMaterials, this);
	}
	++BuildCount;
}

void ACambrianaLiveActor::ShowSettingsUsed()
{
	if (!LastBuildReport.bHasSummary)
	{
		return;
	}
	// A setting whose override is off shows the value the build used, so turning the override on changes nothing.
	if (!bOverrideRecursion)
	{
		Recursion = LastBuildReport.Recursion;
	}
	if (!bOverrideBasicAngle)
	{
		BasicAngle = LastBuildReport.BasicAngle;
	}
	if (!bOverrideThickness)
	{
		Thickness = LastBuildReport.Thickness;
	}
	if (!bOverrideSize)
	{
		SizeMode = LastBuildReport.SizeMode;
		SizeValue = LastBuildReport.SizeValue;
	}
	if (!bOverrideMinThickness)
	{
		MinThickness = LastBuildReport.MinThickness;
	}
	if (!bOverrideClosedForm)
	{
		bClosedForm = LastBuildReport.bClosedForm;
	}
	if (!bOverrideSides)
	{
		Sides = LastBuildReport.Sides;
	}
	LastBuildInput = MakeBuildInput();
}

void ACambrianaLiveActor::RebuildActorsUsing(const UCambrianaGrammar* ChangedGrammar)
{
	if (!ChangedGrammar)
	{
		return;
	}
	for (TObjectIterator<ACambrianaLiveActor> It(RF_ClassDefaultObject | RF_ArchetypeObject); It; ++It)
	{
		if (IsValid(*It) && It->Grammar == ChangedGrammar && It->GetWorld())
		{
			It->RebuildIfInputChanged();
		}
	}
}

void ACambrianaLiveActor::RebuildIfInputChanged()
{
	if (BuildCount == 0 || MakeBuildInput() != LastBuildInput)
	{
		Rebuild();
	}
}

void ACambrianaLiveActor::PostActorCreated()
{
	Super::PostActorCreated();
	Seed = Cambriana::GenerateSeed();
}

void ACambrianaLiveActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildIfInputChanged();
}
