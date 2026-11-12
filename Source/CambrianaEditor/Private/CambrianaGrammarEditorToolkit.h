#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "IDetailsView.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class FCambrianaGrammarPreview;
class IMessageLogListing;
class UCambrianaGrammar;

class FCambrianaGrammarEditorToolkit : public FAssetEditorToolkit
{
public:
	static const FName ToolkitName;
	static const FName AppIdentifier;
	static const FName RulesTabId;
	static const FName ViewportTabId;
	static const FName DetailsTabId;
	static const FName OutputsTabId;
	/** A Rules edit reaches the working copy once typing pauses this long, and in any case on commit, before a save and on close. */
	static constexpr float RulesApplyDelaySeconds = 0.5f;

	void InitGrammarEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCambrianaGrammar* Grammar);
	virtual ~FCambrianaGrammarEditorToolkit() override;

	using FAssetEditorToolkit::GetEditingObject;

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FText GetTabSuffix() const override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;

	/** Asked when the editor closes with unsaved edits: Yes saves them, No discards them, Cancel keeps the editor open. */
	using FUnsavedEditsPrompt = TFunction<EAppReturnType::Type(const FText& Message)>;

	/** The grammar the Rules, Details and Viewport panels edit and show; a save copies it into the asset. */
	UCambrianaGrammar* GetWorkingCopy() const { return WorkingCopy.Get(); }
	/** Whether the working copy differs from the asset or a Rules edit is still waiting to reach it. */
	bool HasUnsavedEdits() const;
	void SetUnsavedEditsPrompt(FUnsavedEditsPrompt Prompt);
	/** Asked by Save As for the new asset's long package name; an empty name cancels. */
	using FSaveAsPrompt = TFunction<FString(const UCambrianaGrammar& Grammar)>;
	void SetSaveAsPrompt(FSaveAsPrompt Prompt);

	FText GetRulesText() const;
	void CommitRulesText(const FText& NewText);

	TArray<TWeakObjectPtr<UObject>> GetDetailsObjects() const;
	const FCambrianaGrammarPreview& GetPreview() const { return *Preview; }
	/** The preview build's diagnostics, shown under the Rules text. */
	const IMessageLogListing& GetRulesDiagnostics() const { return *RulesDiagnostics; }
	/** Whether Details shows the property, given the properties it is nested in, innermost first. */
	bool IsDetailsPropertyVisible(FName PropertyName, TConstArrayView<FName> ParentPropertyNames = {}) const;

	FOnTextCommitted MakeRulesCommittedDelegate();
	FIsPropertyVisible MakeDetailsPropertyVisibleDelegate();

private:
	UCambrianaGrammar* GetGrammar() const;
	TSharedRef<SDockTab> SpawnRulesTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnOutputsTab(const FSpawnTabArgs& Args);
	void HandleGrammarChanged(const UCambrianaGrammar* ChangedGrammar);
	void HandleRulesTextChanged(const FText& NewText);
	void HandleRulesTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	bool HandleRulesApplyTimer(float DeltaTime);
	bool HandleIsDetailsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const;
	void ApplyPendingRulesEdit();
	void UndoAfterPendingRulesEdit();
	void RedoAfterPendingRulesEdit();
	/** Saves the working copy into the asset; when the save does not complete the asset is left as it was. */
	bool SaveWorkingCopy();
	void CancelPendingRulesEdit();
	void UpdatePreview(const UCambrianaGrammar& Grammar);

	TSharedPtr<SMultiLineEditableTextBox> RulesTextBox;
	TSharedPtr<IMessageLogListing> RulesDiagnostics;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<FCambrianaGrammarPreview> Preview;
	TStrongObjectPtr<UCambrianaGrammar> WorkingCopy;
	/** Whether the working copy differed from the asset when either last changed. */
	bool bWorkingCopyEdited = false;
	FDelegateHandle GrammarChangedHandle;
	FTSTicker::FDelegateHandle RulesApplyTimer;
	bool bRulesEditPending = false;
	FUnsavedEditsPrompt UnsavedEditsPrompt;
	FSaveAsPrompt SaveAsPrompt;
	bool bClosingForSaveAs = false;
};
