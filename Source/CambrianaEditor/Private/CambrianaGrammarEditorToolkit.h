#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "IDetailsView.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class FObjectPreSaveContext;
class UCambrianaGrammar;

class FCambrianaGrammarEditorToolkit : public FAssetEditorToolkit
{
public:
	static const FName ToolkitName;
	static const FName AppIdentifier;
	static const FName RulesTabId;
	static const FName DetailsTabId;
	/** A Rules edit reaches the grammar once typing pauses this long, and in any case on commit, before a save and on close. */
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
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;

	FText GetRulesText() const;
	void CommitRulesText(const FText& NewText);

	TArray<TWeakObjectPtr<UObject>> GetDetailsObjects() const;
	bool IsDetailsPropertyVisible(FName PropertyName) const;

	FOnTextCommitted MakeRulesCommittedDelegate();
	FIsPropertyVisible MakeDetailsPropertyVisibleDelegate();

private:
	UCambrianaGrammar* GetGrammar() const;
	TSharedRef<SDockTab> SpawnRulesTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	void HandleGrammarChanged(const UCambrianaGrammar* ChangedGrammar);
	void HandleRulesTextChanged(const FText& NewText);
	void HandleRulesTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	bool HandleRulesApplyTimer(float DeltaTime);
	void HandleObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
	bool HandleIsDetailsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const;
	void ApplyPendingRulesEdit();
	void CancelPendingRulesEdit();

	TSharedPtr<SMultiLineEditableTextBox> RulesTextBox;
	TSharedPtr<IDetailsView> DetailsView;
	FDelegateHandle GrammarChangedHandle;
	FDelegateHandle ObjectPreSaveHandle;
	FTSTicker::FDelegateHandle RulesApplyTimer;
	bool bRulesEditPending = false;
	FString RulesTextBeforeEdit;
};
