using UnrealBuildTool;

public class Cambriana : ModuleRules
{
	public Cambriana(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"GeometryFramework",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CambrianaCore",
			"MeshConversion",
			"MeshDescription",
			"Projects",
			"StaticMeshDescription",
		});
	}
}
