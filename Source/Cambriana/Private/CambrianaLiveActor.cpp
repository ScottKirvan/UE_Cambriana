#include "CambrianaLiveActor.h"

#include "CambrianaGrammar.h"
#include "CambrianaSeed.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const FName PreviewColorParameter(TEXT("Color"));
}

ACambrianaLiveActor::ACambrianaLiveActor()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
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
	FCambrianaBuildInput Input;
	Input.GrammarText = Grammar ? Grammar->GrammarText : InlineGrammar;
	Input.Seed = Seed;

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
	Settings.MinThickness = Grammar && !bOverrideMinThickness ? Grammar->MinThickness : MinThickness;
	Settings.bClosedForm = Grammar && !bOverrideClosedForm ? Grammar->bClosedForm : bClosedForm;
	Settings.Sides = Grammar && !bOverrideSides ? Grammar->Sides : Sides;
	Settings.bOverrideGrammarMinThickness = bOverrideMinThickness;
	Settings.bOverrideGrammarSurface = bOverrideClosedForm;
	Settings.bOverrideGrammarSides = bOverrideSides;
	const bool bAssetSize = Grammar && !bOverrideSize;
	Settings.SizeMode = bAssetSize ? Grammar->SizeMode : SizeMode;
	Settings.SizeValue = bAssetSize ? Grammar->SizeValue : SizeValue;
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
		ApplyPreviewMaterials(Result.SlotMaterialIds);
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

void ACambrianaLiveActor::ApplyPreviewMaterials(TConstArrayView<int64> SlotMaterialIds)
{
	PreviewMaterials.SetNum(Cambriana::PreviewPaletteSize);
	TArray<UMaterialInterface*> MaterialSet;
	MaterialSet.Reserve(SlotMaterialIds.Num());
	for (const int64 MaterialId : SlotMaterialIds)
	{
		TObjectPtr<UMaterialInstanceDynamic>& Material = PreviewMaterials[Cambriana::PreviewPaletteIndex(MaterialId)];
		if (!Material && PreviewBaseMaterial)
		{
			Material = UMaterialInstanceDynamic::Create(PreviewBaseMaterial, this);
			Material->SetVectorParameterValue(PreviewColorParameter, Cambriana::PreviewPaletteColor(MaterialId));
		}
		MaterialSet.Add(Material);
	}
	GetDynamicMeshComponent()->ConfigureMaterialSet(MaterialSet);
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
