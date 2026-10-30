#include "CambrianaLiveActor.h"

#include "CambrianaGrammar.h"
#include "CambrianaSeed.h"
#include "Components/DynamicMeshComponent.h"
#include "UObject/UObjectIterator.h"

void ACambrianaLiveActor::SetGrammar(UCambrianaGrammar* NewGrammar)
{
	Grammar = NewGrammar;
	Rebuild();
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
	Settings.MinThickness = MinThickness;
	Settings.bClosedForm = bClosedForm;
	Settings.Sides = Sides;
	Settings.SizeMode = SizeMode;
	Settings.SizeValue = SizeValue;
	return Input;
}

void ACambrianaLiveActor::Rebuild()
{
	LastBuildInput = MakeBuildInput();
	FCambrianaBuildResult Result = Cambriana::BuildMesh(LastBuildInput);
	if (UDynamicMeshComponent* MeshComponent = GetDynamicMeshComponent())
	{
		MeshComponent->SetMesh(MoveTemp(Result.Mesh));
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
			It->Rebuild();
		}
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
	if (BuildCount == 0 || MakeBuildInput() != LastBuildInput)
	{
		Rebuild();
	}
}
