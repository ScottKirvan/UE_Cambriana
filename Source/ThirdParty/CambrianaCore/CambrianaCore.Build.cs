using System.IO;
using UnrealBuildTool;

public class CambrianaCore : ModuleRules
{
	public CambrianaCore(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win64", "CambrianaCore.lib"));
			PublicDelayLoadDLLs.Add("CambrianaCore.dll");
			RuntimeDependencies.Add("$(BinaryOutputDir)/CambrianaCore.dll", Path.Combine(ModuleDirectory, "bin", "Win64", "CambrianaCore.dll"));
		}
	}
}
