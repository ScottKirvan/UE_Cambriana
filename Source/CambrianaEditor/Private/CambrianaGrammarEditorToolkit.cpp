#include "CambrianaGrammarEditorToolkit.h"

#include "CambrianaGrammar.h"
#include "CambrianaGrammarPreview.h"
#include "CambrianaGrammarWorkingCopy.h"
#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SCambrianaGrammarViewport.h"
#include "ScopedTransaction.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Layout/SSplitter.h"

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
	WorkingCopy.Reset(Cambriana::Editor::DuplicateWorkingCopy(*Grammar));
	UnsavedEditsPrompt = [](const FText& Message) { return FMessageDialog::Open(EAppMsgType::YesNoCancel, Message); };
	SaveAsPrompt = [](const UCambrianaGrammar& Edited)
	{
		FSaveAssetDialogConfig Config;
		Config.DefaultPath = FPackageName::GetLongPackagePath(Edited.GetPackage()->GetName());
		Config.DefaultAssetName = Edited.GetName();
		Config.AssetClassNames.Add(UCambrianaGrammar::StaticClass()->GetClassPathName());
		Config.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		Config.DialogTitleOverride = LOCTEXT("SaveAsTitle", "Save Grammar As");
		const FString ObjectPath = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreateModalSaveAssetDialog(Config);
		return ObjectPath.IsEmpty() ? FString() : FPackageName::ObjectPathToPackageName(ObjectPath);
	};

	RulesTextBox = SNew(SMultiLineEditableTextBox)
		.Text(FText::FromString(WorkingCopy->GrammarText))
		.OnTextChanged(FOnTextChanged::CreateSP(this, &FCambrianaGrammarEditorToolkit::HandleRulesTextChanged))
		.OnTextCommitted(MakeRulesCommittedDelegate());

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
	DetailsView->SetIsPropertyVisibleDelegate(MakeDetailsPropertyVisibleDelegate());
	DetailsView->SetObject(WorkingCopy.Get());

	FMessageLogInitializationOptions DiagnosticsOptions;
	DiagnosticsOptions.bShowFilters = false;
	DiagnosticsOptions.bShowPages = false;
	DiagnosticsOptions.bAllowClear = false;
	DiagnosticsOptions.MaxPageCount = 1;
	RulesDiagnostics = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog")).CreateLogListing(TEXT("CambrianaGrammarDiagnostics"), DiagnosticsOptions);

	Preview = MakeShared<FCambrianaGrammarPreview>();
	UpdatePreview(*WorkingCopy);

	GrammarChangedHandle = UCambrianaGrammar::OnGrammarChanged().AddSP(this, &FCambrianaGrammarEditorToolkit::HandleGrammarChanged);

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

FText FCambrianaGrammarEditorToolkit::GetTabSuffix() const
{
	return HasUnsavedEdits() ? LOCTEXT("UnsavedEdits", "*") : FAssetEditorToolkit::GetTabSuffix();
}

bool FCambrianaGrammarEditorToolkit::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	ApplyPendingRulesEdit();
	const UCambrianaGrammar* Grammar = GetGrammar();
	if (Grammar && !bClosingForSaveAs && InCloseReason != EAssetEditorCloseReason::AssetForceDeleted && HasUnsavedEdits())
	{
		const FText Message = FText::Format(LOCTEXT("UnsavedEditsPrompt", "Save your changes to {0}?\n(Selecting 'No' discards them.)"),
			FText::FromString(Grammar->GetPathName()));
		switch (UnsavedEditsPrompt(Message))
		{
		case EAppReturnType::Yes:
			if (!SaveWorkingCopy())
			{
				return false;
			}
			break;
		case EAppReturnType::No:
			break;
		default:
			return false;
		}
	}
	return FAssetEditorToolkit::OnRequestClose(InCloseReason);
}

void FCambrianaGrammarEditorToolkit::SaveAsset_Execute()
{
	SaveWorkingCopy();
}

void FCambrianaGrammarEditorToolkit::SaveAssetAs_Execute()
{
	ApplyPendingRulesEdit();
	UCambrianaGrammar* Grammar = GetGrammar();
	const FString NewPackageName = Grammar ? SaveAsPrompt(*Grammar) : FString();
	if (NewPackageName.IsEmpty())
	{
		return;
	}
	if (NewPackageName == Grammar->GetPackage()->GetName())
	{
		SaveWorkingCopy();
		return;
	}
	FText InvalidName;
	if (!FFileHelper::IsFilenameValidForSaving(NewPackageName, InvalidName))
	{
		FMessageDialog::Open(EAppMsgType::Ok, InvalidName);
		return;
	}
	UCambrianaGrammar* NewGrammar = Cambriana::Editor::DuplicateAsAsset(*WorkingCopy, NewPackageName);
	if (!NewGrammar || FEditorFileUtils::PromptForCheckoutAndSave({ NewGrammar->GetPackage() }, false, false) != FEditorFileUtils::PR_Success)
	{
		return;
	}
	// Closing releases the editor's last reference to itself.
	const TSharedRef<FCambrianaGrammarEditorToolkit> KeepAlive = SharedThis(this);
	const TSharedPtr<IToolkitHost> Host = ToolkitHost.Pin();
	UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	bClosingForSaveAs = true;
	AssetEditors->CloseAllEditorsForAsset(Grammar);
	AssetEditors->OpenEditorForAsset(NewGrammar, ToolkitMode, Host);
	OnAssetsSavedAs({ NewGrammar });
}

bool FCambrianaGrammarEditorToolkit::HasUnsavedEdits() const
{
	const UCambrianaGrammar* Grammar = GetGrammar();
	return bRulesEditPending || (Grammar && !Cambriana::Editor::HoldSameGrammar(*WorkingCopy, *Grammar));
}

void FCambrianaGrammarEditorToolkit::SetUnsavedEditsPrompt(FUnsavedEditsPrompt Prompt)
{
	UnsavedEditsPrompt = MoveTemp(Prompt);
}

void FCambrianaGrammarEditorToolkit::SetSaveAsPrompt(FSaveAsPrompt Prompt)
{
	SaveAsPrompt = MoveTemp(Prompt);
}

FText FCambrianaGrammarEditorToolkit::GetRulesText() const
{
	return RulesTextBox->GetText();
}

void FCambrianaGrammarEditorToolkit::CommitRulesText(const FText& NewText)
{
	CancelPendingRulesEdit();
	const FString NewGrammarText = ToGrammarText(NewText.ToString());
	if (WorkingCopy->GrammarText.Equals(NewGrammarText, ESearchCase::CaseSensitive))
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("EditRules", "Edit Grammar Rules"));
	FProperty* Property = UCambrianaGrammar::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UCambrianaGrammar, GrammarText));
	WorkingCopy->PreEditChange(Property);
	WorkingCopy->GrammarText = NewGrammarText;
	FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
	WorkingCopy->PostEditChangeProperty(Event);
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
	return SNew(SDockTab).Label(LOCTEXT("RulesTitle", "Rules"))
	[
		SNew(SSplitter).Orientation(Orient_Vertical)
		+ SSplitter::Slot().Value(0.8f)[RulesTextBox.ToSharedRef()]
		+ SSplitter::Slot().Value(0.2f)[FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog")).CreateLogListingWidget(RulesDiagnostics.ToSharedRef())]
	];
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
	UCambrianaGrammar* Grammar = GetGrammar();
	if (!ChangedGrammar || !Grammar || (ChangedGrammar != Grammar && ChangedGrammar != WorkingCopy.Get()))
	{
		return;
	}
	// Copying into the working copy broadcasts its own change, which does the rest.
	if (ChangedGrammar == Grammar && !bRulesEditPending && !bWorkingCopyEdited && Cambriana::Editor::CopyGrammar(*Grammar, *WorkingCopy))
	{
		return;
	}
	bWorkingCopyEdited = !Cambriana::Editor::HoldSameGrammar(*WorkingCopy, *Grammar);
	if (ChangedGrammar != WorkingCopy.Get())
	{
		return;
	}
	UpdatePreview(*ChangedGrammar);
	// A change that left the grammar text alone keeps a Rules edit still waiting to be applied.
	if (bRulesEditPending && ChangedGrammar->GrammarText.Equals(RulesTextBeforeEdit, ESearchCase::CaseSensitive))
	{
		return;
	}
	if (!ToGrammarText(RulesTextBox->GetText().ToString()).Equals(ToGrammarText(ChangedGrammar->GrammarText), ESearchCase::CaseSensitive))
	{
		RulesTextBox->SetText(FText::FromString(ChangedGrammar->GrammarText));
	}
	// Setting the text reports it as typed, though it already matches the working copy.
	CancelPendingRulesEdit();
}

void FCambrianaGrammarEditorToolkit::HandleRulesTextChanged(const FText& NewText)
{
	if (!bRulesEditPending)
	{
		bRulesEditPending = true;
		RulesTextBeforeEdit = WorkingCopy->GrammarText;
	}
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

bool FCambrianaGrammarEditorToolkit::SaveWorkingCopy()
{
	ApplyPendingRulesEdit();
	UCambrianaGrammar* Grammar = GetGrammar();
	if (!Grammar)
	{
		return false;
	}
	if (!Grammar->IsAsset())
	{
		// Nothing on disk to write.
		Cambriana::Editor::CopyGrammar(*WorkingCopy, *Grammar);
		return true;
	}
	// The asset takes the edits for the save without a change event, so its actors rebuild only once it is saved.
	UPackage* Package = Grammar->GetPackage();
	const bool bWasDirty = Package->IsDirty();
	UCambrianaGrammar* BeforeSave = NewObject<UCambrianaGrammar>(GetTransientPackage());
	const TStrongObjectPtr<UCambrianaGrammar> KeepBeforeSave(BeforeSave);
	Cambriana::Editor::CopyGrammarValues(*Grammar, *BeforeSave);
	const bool bChanged = Cambriana::Editor::CopyGrammarValues(*WorkingCopy, *Grammar);
	if (FEditorFileUtils::PromptForCheckoutAndSave({ Package }, false, false) != FEditorFileUtils::PR_Success)
	{
		Cambriana::Editor::CopyGrammarValues(*BeforeSave, *Grammar);
		Package->SetDirtyFlag(bWasDirty);
		return false;
	}
	if (bChanged)
	{
		Cambriana::Editor::NotifyGrammarChanged(*Grammar);
	}
	OnAssetsSaved({ Grammar });
	return true;
}

void FCambrianaGrammarEditorToolkit::CancelPendingRulesEdit()
{
	FTSTicker::RemoveTicker(RulesApplyTimer);
	RulesApplyTimer.Reset();
	bRulesEditPending = false;
	RulesTextBeforeEdit.Reset();
}

void FCambrianaGrammarEditorToolkit::UpdatePreview(const UCambrianaGrammar& Grammar)
{
	Preview->Update(Grammar);
	TArray<TSharedRef<FTokenizedMessage>> Messages;
	for (const FCambrianaDiagnostic& Diagnostic : Preview->GetLastBuildReport().Diagnostics)
	{
		const EMessageSeverity::Type Severity = Diagnostic.Severity == ECambrianaDiagnosticSeverity::Error ? EMessageSeverity::Error : EMessageSeverity::Warning;
		Messages.Add(FTokenizedMessage::Create(Severity, FText::FromString(Diagnostic.DescribePositionAndProblem())));
	}
	RulesDiagnostics->ClearMessages();
	// The build has already logged its diagnostics.
	RulesDiagnostics->AddMessages(Messages, false);
}

#undef LOCTEXT_NAMESPACE
