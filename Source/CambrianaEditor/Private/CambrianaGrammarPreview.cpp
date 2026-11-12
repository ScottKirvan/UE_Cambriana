#include "CambrianaGrammarPreview.h"

#include "AdvancedPreviewScene.h"
#include "CambrianaGrammar.h"
#include "CambrianaLiveActor.h"
#include "CambrianaPreviewMaterials.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

FCambrianaGrammarPreview::FCambrianaGrammarPreview()
	: Scene(MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues()))
{
	MeshComponent = NewObject<UDynamicMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Scene->AddComponent(MeshComponent, FTransform::Identity);
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, Cambriana::PreviewBaseMaterialPath);
}

FCambrianaGrammarPreview::~FCambrianaGrammarPreview()
{
	Scene->RemoveComponent(MeshComponent);
}

void FCambrianaGrammarPreview::Update(const UCambrianaGrammar& Grammar)
{
	FCambrianaBuildInput Input = ACambrianaLiveActor::MakeBuildInputFor(Grammar);
	if (BuildCount > 0 && Input == LastBuildInput)
	{
		return;
	}
	LastBuildInput = MoveTemp(Input);
	FCambrianaBuildResult Result = Cambriana::BuildMesh(LastBuildInput);
	LastBuildReport = MoveTemp(Result.Report);
	MeshComponent->SetMesh(MoveTemp(Result.Mesh));
	Cambriana::ApplyPreviewMaterials(*MeshComponent, Result.SlotMaterialIds, BaseMaterial, Palette, MeshComponent);
	++BuildCount;
}

UWorld* FCambrianaGrammarPreview::GetWorld() const
{
	return Scene->GetWorld();
}

void FCambrianaGrammarPreview::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MeshComponent);
	Collector.AddReferencedObject(BaseMaterial);
	Collector.AddReferencedObjects(Palette);
}

FString FCambrianaGrammarPreview::GetReferencerName() const
{
	return TEXT("FCambrianaGrammarPreview");
}
