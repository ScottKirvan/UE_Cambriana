using UnrealBuildTool;

public class CambrianaEditor : ModuleRules
{
	public CambrianaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetDefinition",
			"Cambriana",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"Engine",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd",
		});
	}
}
