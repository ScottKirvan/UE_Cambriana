#include "CambrianaGrammarEditorToolkit.h"

#include "CambrianaGrammar.h"
#include "CambrianaGrammarPreview.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SCambrianaGrammarViewport.h"
#include "ScopedTransaction.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "CambrianaGrammarEditor"

namespace
{
	// Slate text boxes join lines with the platform's line terminator, "\r\n" on Windows.
	FString ToGrammarText(const FString& RulesText)
	{
		return RulesText.Replace(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
	}
}

const FName FCambrianaGrammarEditorToolkit::ToolkitName(TEXT("CambrianaGrammarEditor"));
const FName FCambrianaGrammarEditorToolkit::AppIdentifier(TEXT("CambrianaGrammarEditorApp"));
const FName FCambrianaGrammarEditorToolkit::RulesTabId(TEXT("CambrianaGrammarEditor_Rules"));
const FName FCambrianaGrammarEditorToolkit::ViewportTabId(TEXT("CambrianaGrammarEditor_Viewport"));
const FName FCambrianaGrammarEditorToolkit::DetailsTabId(TEXT("CambrianaGrammarEditor_Details"));
const FName FCambrianaGrammarEditorToolkit::OutputsTabId(TEXT("CambrianaGrammarEditor_Outputs"));

void FCambrianaGrammarEditorToolkit::InitGrammarEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UCambrianaGrammar* Grammar)
{
	RulesTextBox = SNew(SMultiLineEditableTextBox)
		.Text(FText::FromString(Grammar->GrammarText))
		.OnTextChanged(FOnTextChanged::CreateSP(this, &FCambrianaGrammarEditorToolkit::HandleRulesTextChanged))
		.OnTextCommitted(MakeRulesCommittedDelegate());

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
	DetailsView->SetIsPropertyVisibleDelegate(MakeDetailsPropertyVisibleDelegate());
	DetailsView->SetObject(Grammar);

	Preview = MakeShared<FCambrianaGrammarPreview>();
	Preview->Update(*Grammar);

	GrammarChangedHandle = UCambrianaGrammar::OnGrammarChanged().AddSP(this, &FCambrianaGrammarEditorToolkit::HandleGrammarChanged);
	ObjectPreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddSP(this, &FCambrianaGrammarEditorToolkit::HandleObjectPreSave);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_CambrianaGrammarEditor_Layout_v2"))
		->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.3f)->AddTab(RulesTabId, ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.45f)->AddTab(ViewportTabId, ETabState::OpenedTab)->SetHideTabWell(true))
			->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.25f)
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.7f)->AddTab(DetailsTabId, ETabState::OpenedTab))
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.3f)->AddTab(OutputsTabId, ETabState::OpenedTab))));

	InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, Layout, true, true, Grammar);
}

FCambrianaGrammarEditorToolkit::~FCambrianaGrammarEditorToolkit()
{
	UCambrianaGrammar::OnGrammarChanged().Remove(GrammarChangedHandle);
	FCoreUObjectDelegates::OnObjectPreSave.Remove(ObjectPreSaveHandle);
	FTSTicker::RemoveTicker(RulesApplyTimer);
}

void FCambrianaGrammarEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Cambriana Grammar Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(RulesTabId, FOnSpawnTab::CreateSP(this, &FCambrianaGrammarEditorToolkit::SpawnRulesTab))
		.SetDisplayName(LOCTEXT("RulesTab", "Rules"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FCambrianaGrammarEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FCambrianaGrammarEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(OutputsTabId, FOnSpawnTab::CreateSP(this, &FCambrianaGrammarEditorToolkit::SpawnOutputsTab))
		.SetDisplayName(LOCTEXT("OutputsTab", "Outputs"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FCambrianaGrammarEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(RulesTabId);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(OutputsTabId);
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

bool FCambrianaGrammarEditorToolkit::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	ApplyPendingRulesEdit();
	return FAssetEditorToolkit::OnRequestClose(InCloseReason);
}

void FCambrianaGrammarEditorToolkit::SaveAsset_Execute()
{
	ApplyPendingRulesEdit();
	FAssetEditorToolkit::SaveAsset_Execute();
}

void FCambrianaGrammarEditorToolkit::SaveAssetAs_Execute()
{
	ApplyPendingRulesEdit();
	FAssetEditorToolkit::SaveAssetAs_Execute();
}

FText FCambrianaGrammarEditorToolkit::GetRulesText() const
{
	return RulesTextBox->GetText();
}

void FCambrianaGrammarEditorToolkit::CommitRulesText(const FText& NewText)
{
	CancelPendingRulesEdit();
	UCambrianaGrammar* Grammar = GetGrammar();
	const FString NewGrammarText = ToGrammarText(NewText.ToString());
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

FOnTextCommitted FCambrianaGrammarEditorToolkit::MakeRulesCommittedDelegate()
{
	return FOnTextCommitted::CreateSP(this, &FCambrianaGrammarEditorToolkit::HandleRulesTextCommitted);
}

FIsPropertyVisible FCambrianaGrammarEditorToolkit::MakeDetailsPropertyVisibleDelegate()
{
	return FIsPropertyVisible::CreateSP(this, &FCambrianaGrammarEditorToolkit::HandleIsDetailsPropertyVisible);
}

UCambrianaGrammar* FCambrianaGrammarEditorToolkit::GetGrammar() const
{
	return Cast<UCambrianaGrammar>(GetEditingObject());
}

TSharedRef<SDockTab> FCambrianaGrammarEditorToolkit::SpawnRulesTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("RulesTitle", "Rules"))[RulesTextBox.ToSharedRef()];
}

TSharedRef<SDockTab> FCambrianaGrammarEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("ViewportTitle", "Viewport"))[SNew(SCambrianaGrammarViewport, Preview.ToSharedRef())];
}

TSharedRef<SDockTab> FCambrianaGrammarEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("DetailsTitle", "Details"))[DetailsView.ToSharedRef()];
}

TSharedRef<SDockTab> FCambrianaGrammarEditorToolkit::SpawnOutputsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("OutputsTitle", "Outputs"));
}

void FCambrianaGrammarEditorToolkit::HandleGrammarChanged(const UCambrianaGrammar* ChangedGrammar)
{
	if (ChangedGrammar != GetGrammar())
	{
		return;
	}
	Preview->Update(*ChangedGrammar);
	// A change that left the grammar text alone keeps a Rules edit still waiting to be applied.
	if (bRulesEditPending && ChangedGrammar->GrammarText.Equals(RulesTextBeforeEdit, ESearchCase::CaseSensitive))
	{
		return;
	}
	CancelPendingRulesEdit();
	if (!ToGrammarText(RulesTextBox->GetText().ToString()).Equals(ToGrammarText(ChangedGrammar->GrammarText), ESearchCase::CaseSensitive))
	{
		RulesTextBox->SetText(FText::FromString(ChangedGrammar->GrammarText));
	}
}

void FCambrianaGrammarEditorToolkit::HandleRulesTextChanged(const FText& NewText)
{
	UCambrianaGrammar* Grammar = GetGrammar();
	if (!Grammar)
	{
		return;
	}
	if (!bRulesEditPending)
	{
		bRulesEditPending = true;
		RulesTextBeforeEdit = Grammar->GrammarText;
	}
	Grammar->MarkPackageDirty();
	FTSTicker::RemoveTicker(RulesApplyTimer);
	RulesApplyTimer = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &FCambrianaGrammarEditorToolkit::HandleRulesApplyTimer), RulesApplyDelaySeconds);
}

void FCambrianaGrammarEditorToolkit::HandleRulesTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	CommitRulesText(NewText);
}

bool FCambrianaGrammarEditorToolkit::HandleRulesApplyTimer(float DeltaTime)
{
	RulesApplyTimer.Reset();
	ApplyPendingRulesEdit();
	return false;
}

void FCambrianaGrammarEditorToolkit::HandleObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext)
{
	if (Object && Object == GetGrammar())
	{
		ApplyPendingRulesEdit();
	}
}

bool FCambrianaGrammarEditorToolkit::HandleIsDetailsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	return IsDetailsPropertyVisible(PropertyAndParent.Property.GetFName());
}

void FCambrianaGrammarEditorToolkit::ApplyPendingRulesEdit()
{
	if (bRulesEditPending)
	{
		CommitRulesText(RulesTextBox->GetText());
	}
}

void FCambrianaGrammarEditorToolkit::CancelPendingRulesEdit()
{
	FTSTicker::RemoveTicker(RulesApplyTimer);
	RulesApplyTimer.Reset();
	bRulesEditPending = false;
	RulesTextBeforeEdit.Reset();
}

#undef LOCTEXT_NAMESPACE
