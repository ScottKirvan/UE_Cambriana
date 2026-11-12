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
		const ACambrianaLiveActor* Default = GetDefault<ACambrianaLiveActor>();
		const FCambrianaStatedSettings Stated = Cambriana::ReadStatedSettings(Source->GrammarText);
		Recursion = Stated.Recursion.Get(Default->Recursion);
		BasicAngle = Stated.BasicAngle.Get(Default->BasicAngle);
		Thickness = Stated.Thickness.Get(Default->Thickness);
		SizeMode = Stated.SizeMode.Get(Source->SizeMode);
		SizeValue = Stated.SizeValue.Get(Source->SizeValue);
		Sides = Stated.Sides.Get(Source->Sides);
		bClosedForm = Stated.bClosedForm.Get(Source->bClosedForm);
		MinThickness = Stated.MinThickness.Get(Source->MinThickness);
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
	if (InGrammar)
	{
		Input.ParameterValues = InGrammar->ParameterValues;
	}

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
	if (UDynamicMeshComponent* MeshComponent = GetDynamicMeshComponent())
	{
		MeshComponent->SetMesh(MoveTemp(Result.Mesh));
		Cambriana::ApplyPreviewMaterials(*MeshComponent, Result.SlotMaterialIds, PreviewBaseMaterial, PreviewMaterials, this);
	}
	++BuildCount;
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
