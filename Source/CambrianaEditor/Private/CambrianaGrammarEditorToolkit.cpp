#include "CambrianaGrammarEditorToolkit.h"

#include "CambrianaGrammar.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "CambrianaGrammarEditor"

const FName FCambrianaGrammarEditorToolkit::ToolkitName(TEXT("CambrianaGrammarEditor"));
const FName FCambrianaGrammarEditorToolkit::AppIdentifier(TEXT("CambrianaGrammarEditorApp"));
const FName FCambrianaGrammarEditorToolkit::RulesTabId(TEXT("CambrianaGrammarEditor_Rules"));
const FName FCambrianaGrammarEditorToolkit::DetailsTabId(TEXT("CambrianaGrammarEditor_Details"));

void FCambrianaGrammarEditorToolkit::InitGrammarEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UCambrianaGrammar* Grammar)
{
	RulesTextBox = SNew(SMultiLineEditableTextBox)
		.Text(FText::FromString(Grammar->GrammarText))
		.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type) { CommitRulesText(NewText); });

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(
		[this](const FPropertyAndParent& PropertyAndParent) { return IsDetailsPropertyVisible(PropertyAndParent.Property.GetFName()); }));
	DetailsView->SetObject(Grammar);

	GrammarChangedHandle = UCambrianaGrammar::OnGrammarChanged().AddRaw(this, &FCambrianaGrammarEditorToolkit::HandleGrammarChanged);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_CambrianaGrammarEditor_Layout_v1"))
		->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.6f)->AddTab(RulesTabId, ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.4f)->AddTab(DetailsTabId, ETabState::OpenedTab)));

	InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, Layout, true, true, Grammar);
}

FCambrianaGrammarEditorToolkit::~FCambrianaGrammarEditorToolkit()
{
	UCambrianaGrammar::OnGrammarChanged().Remove(GrammarChangedHandle);
}

void FCambrianaGrammarEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Cambriana Grammar Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(RulesTabId, FOnSpawnTab::CreateSP(this, &FCambrianaGrammarEditorToolkit::SpawnRulesTab))
		.SetDisplayName(LOCTEXT("RulesTab", "Rules"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FCambrianaGrammarEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FCambrianaGrammarEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(RulesTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

FName FCambrianaGrammarEditorToolkit::GetToolkitFName() const
{
	return ToolkitName;
}

FText FCambrianaGrammarEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Cambriana Grammar Editor");
}

FString FCambrianaGrammarEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("CambrianaGrammar ");
}

FLinearColor FCambrianaGrammarEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(FColor(64, 160, 96));
}

FText FCambrianaGrammarEditorToolkit::GetRulesText() const
{
	return RulesTextBox->GetText();
}

void FCambrianaGrammarEditorToolkit::CommitRulesText(const FText& NewText)
{
	UCambrianaGrammar* Grammar = GetGrammar();
	const FString NewGrammarText = NewText.ToString();
	if (!Grammar || Grammar->GrammarText.Equals(NewGrammarText, ESearchCase::CaseSensitive))
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("EditRules", "Edit Grammar Rules"));
	FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText));
	Grammar->PreEditChange(Property);
	Grammar->GrammarText = NewGrammarText;
	FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
	Grammar->PostEditChangeProperty(Event);
}

TArray<TWeakObjectPtr<UObject>> FCambrianaGrammarEditorToolkit::GetDetailsObjects() const
{
	return DetailsView->GetSelectedObjects();
}

bool FCambrianaGrammarEditorToolkit::IsDetailsPropertyVisible(FName PropertyName) const
{
	return PropertyName != GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText);
}

UCambrianaGrammar* FCambrianaGrammarEditorToolkit::GetGrammar() const
{
	return Cast<UCambrianaGrammar>(GetEditingObject());
}

TSharedRef<SDockTab> FCambrianaGrammarEditorToolkit::SpawnRulesTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("RulesTitle", "Rules"))[RulesTextBox.ToSharedRef()];
}

TSharedRef<SDockTab> FCambrianaGrammarEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("DetailsTitle", "Details"))[DetailsView.ToSharedRef()];
}

void FCambrianaGrammarEditorToolkit::HandleGrammarChanged(const UCambrianaGrammar* ChangedGrammar)
{
	if (ChangedGrammar == GetGrammar() && !RulesTextBox->GetText().ToString().Equals(ChangedGrammar->GrammarText, ESearchCase::CaseSensitive))
	{
		RulesTextBox->SetText(FText::FromString(ChangedGrammar->GrammarText));
	}
}

#undef LOCTEXT_NAMESPACE
