using UnrealBuildTool;

public class CambrianaEditor : ModuleRules
{
	public CambrianaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Cambriana",
		});
	}
}
