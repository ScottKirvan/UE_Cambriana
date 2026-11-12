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
			"AssetTools",
			"Cambriana",
			"CambrianaCore",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"GeometryCore",
			"GeometryFramework",
			"Engine",
			"PlacementMode",
			"Projects",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd",
		});
	}
}
