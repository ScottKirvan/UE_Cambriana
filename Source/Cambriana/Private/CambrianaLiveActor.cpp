#include "CambrianaLiveActor.h"

#include "CambrianaGrammar.h"
#include "CambrianaGrammarFormat.h"
#include "CambrianaPreviewMaterials.h"
#include "CambrianaSeed.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
	template <typename FActor, typename FVisit>
	void ForEachOverridableSetting(FActor& Actor, const FCambrianaOverridableSettings& Starting, FVisit&& Visit)
	{
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Recursion), Actor.Recursion, Starting.Recursion);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle), Actor.BasicAngle, Starting.BasicAngle);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Thickness), Actor.Thickness, Starting.Thickness);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode), Actor.SizeMode, Starting.SizeMode);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue), Actor.SizeValue, Starting.SizeValue);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, MinThickness), Actor.MinThickness, Starting.MinThickness);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm), Actor.bClosedForm, Starting.bClosedForm);
		Visit(GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides), Actor.Sides, Starting.Sides);
	}
}

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
	Grammar = Source;
	if (Source)
	{
		Seed = Source->Seed;
		ForEachOverridableSetting(*this, GetStartingSettings(), [](FName, auto& Value, const auto& Starting) { Value = Starting; });
	}
	Rebuild();
}

FCambrianaOverridableSettings ACambrianaLiveActor::GetStartingSettings() const
{
	const ACambrianaLiveActor* Default = GetDefault<ACambrianaLiveActor>();
	const FCambrianaStatedSettings& Stated = GetStatedSettings();
	FCambrianaOverridableSettings Starting;
	Starting.Recursion = Stated.Recursion.Get(Default->Recursion);
	Starting.BasicAngle = Stated.BasicAngle.Get(Default->BasicAngle);
	Starting.Thickness = Stated.Thickness.Get(Default->Thickness);
	Starting.SizeMode = Stated.SizeMode.Get(Grammar ? Grammar->SizeMode : Default->SizeMode);
	Starting.SizeValue = Stated.SizeValue.Get(Grammar ? Grammar->SizeValue : Default->SizeValue);
	Starting.Sides = Stated.Sides.Get(Grammar ? Grammar->Sides : Default->Sides);
	Starting.bClosedForm = Stated.bClosedForm.Get(Grammar ? Grammar->bClosedForm : Default->bClosedForm);
	Starting.MinThickness = Stated.MinThickness.Get(Grammar ? Grammar->MinThickness : Default->MinThickness);
	return Starting;
}

const FCambrianaStatedSettings& ACambrianaLiveActor::GetStatedSettings() const
{
	const FString& Text = Grammar ? Grammar->GrammarText : InlineGrammar;
	if (!Text.Equals(StatedSettingsText, ESearchCase::CaseSensitive))
	{
		StatedSettings = Cambriana::ReadStatedSettings(Text);
		StatedSettingsText = Text;
	}
	return StatedSettings;
}

TConstArrayView<FName> ACambrianaLiveActor::GetStartingValueProperties()
{
	static const TArray<FName> Names = []
	{
		TArray<FName> Collected;
		ForEachOverridableSetting(*GetDefault<ACambrianaLiveActor>(), FCambrianaOverridableSettings(),
			[&Collected](FName Name, const auto&, const auto&) { Collected.Add(Name); });
		return Collected;
	}();
	return Names;
}

bool ACambrianaLiveActor::IsAtStartingValue(FName PropertyName) const
{
	bool bAtStartingValue = true;
	ForEachOverridableSetting(*this, GetStartingSettings(), [PropertyName, &bAtStartingValue](FName Name, const auto& Value, const auto& Starting)
	{
		if (Name == PropertyName)
		{
			bAtStartingValue = Value == Starting;
		}
	});
	return bAtStartingValue;
}

void ACambrianaLiveActor::ResetToStartingValue(FName PropertyName)
{
	ForEachOverridableSetting(*this, GetStartingSettings(), [PropertyName](FName Name, auto& Value, const auto& Starting)
	{
		if (Name == PropertyName)
		{
			Value = Starting;
		}
	});
}

bool ACambrianaLiveActor::UsesOwnValue(FName PropertyName) const
{
	const FCambrianaStatedSettings& Stated = GetStatedSettings();
	const bool bInline = Grammar == nullptr;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Recursion))
	{
		return bOverrideRecursion;
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, BasicAngle))
	{
		return bOverrideBasicAngle;
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Thickness))
	{
		return bOverrideThickness;
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, MinThickness))
	{
		return bOverrideMinThickness || (bInline && !Stated.MinThickness.IsSet());
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, bClosedForm))
	{
		return bOverrideClosedForm || (bInline && !Stated.bClosedForm.IsSet());
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, Sides))
	{
		return bOverrideSides || (bInline && !Stated.Sides.IsSet());
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeMode) || PropertyName == GET_MEMBER_NAME_CHECKED(ACambrianaLiveActor, SizeValue))
	{
		return bOverrideSize || (bInline && Cambriana::DetectGrammarFormat(InlineGrammar) == ECambrianaGrammarFormat::Legacy);
	}
	return false;
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
