using UnrealBuildTool;

public class CambrianaEditor : ModuleRules
{
	public CambrianaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AdvancedPreviewScene",
			"AssetDefinition",
			"AssetRegistry",
			"AssetTools",
			"Cambriana",
			"CambrianaCore",
			"Core",
			"ContentBrowser",
			"CoreUObject",
			"EditorFramework",
			"GeometryCore",
			"GeometryFramework",
			"InputCore",
			"Engine",
			"MessageLog",
			"PlacementMode",
			"Projects",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd",
		});
	}
}
