using UnrealBuildTool;

public class CambrianaEditor : ModuleRules
{
	public CambrianaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetDefinition",
			"AssetTools",
			"Cambriana",
			"CambrianaCore",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"Engine",
			"Projects",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd",
		});
	}
}
