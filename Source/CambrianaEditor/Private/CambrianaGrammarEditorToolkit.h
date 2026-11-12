#pragma once

#include "CoreMinimal.h"
#include "IDetailsView.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class UCambrianaGrammar;

class FCambrianaGrammarEditorToolkit : public FAssetEditorToolkit
{
public:
	static const FName ToolkitName;
	static const FName AppIdentifier;
	static const FName RulesTabId;
	static const FName DetailsTabId;

	void InitGrammarEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCambrianaGrammar* Grammar);
	virtual ~FCambrianaGrammarEditorToolkit() override;

	using FAssetEditorToolkit::GetEditingObject;

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

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

	TSharedPtr<SMultiLineEditableTextBox> RulesTextBox;
	TSharedPtr<IDetailsView> DetailsView;
	FDelegateHandle GrammarChangedHandle;
};
