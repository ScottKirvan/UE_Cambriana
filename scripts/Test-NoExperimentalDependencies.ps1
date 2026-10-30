<#
ATS-05: fails if the plugin descriptor or any of the plugin's modules depends directly on an
experimental engine plugin (one whose descriptor sets IsExperimentalVersion to true).
Module dependencies come from UnrealBuildTool's JSON export of the editor target; pass
-TargetJson to check an existing export instead of running one.
#>
param(
	[string]$ProjectFile = 'D:\3\ue\5.8\dev\Cambriana_Base\Cambriana_Base.uproject',
	[string]$EngineDir = 'D:\bin\Epic Games\UE_5.8',
	[string]$PluginDescriptor = (Join-Path $PSScriptRoot '..\Cambriana.uplugin'),
	[string]$TargetJson
)

$ErrorActionPreference = 'Stop'

function Test-Experimental([string]$UpluginPath) {
	# Engine descriptors may contain trailing commas, which ConvertFrom-Json rejects.
	(Get-Content -LiteralPath $UpluginPath -Raw) -match '"IsExperimentalVersion"\s*:\s*true'
}

function Find-OwningPlugin([string]$Directory) {
	$dir = Get-Item -LiteralPath $Directory
	while ($dir) {
		$uplugin = Get-ChildItem -LiteralPath $dir.FullName -Filter *.uplugin -File | Select-Object -First 1
		if ($uplugin) { return $uplugin }
		$dir = $dir.Parent
	}
	$null
}

if ($TargetJson) {
	$modules = (Get-Content -LiteralPath $TargetJson -Raw | ConvertFrom-Json).Modules
}
else {
	$TargetJson = Join-Path ([IO.Path]::GetTempPath()) "Cambriana-target-$PID.json"
	try {
		& (Join-Path $EngineDir 'Engine\Build\BatchFiles\Build.bat') UnrealEditor Win64 Development "-Project=$ProjectFile" -Mode=JsonExport "-OutputFile=$TargetJson" | Out-Null
		if ($LASTEXITCODE -ne 0) { throw "JSON export of the editor target failed ($LASTEXITCODE)" }
		$modules = (Get-Content -LiteralPath $TargetJson -Raw | ConvertFrom-Json).Modules
	}
	finally {
		Remove-Item -LiteralPath $TargetJson -ErrorAction SilentlyContinue
	}
}

$descriptor = Get-Content -LiteralPath $PluginDescriptor -Raw | ConvertFrom-Json
$pluginName = [IO.Path]::GetFileNameWithoutExtension($PluginDescriptor)
$searchDirs = @((Join-Path $EngineDir 'Engine\Plugins'), (Join-Path (Split-Path $ProjectFile) 'Plugins'))

$violations = @()
$checked = 0

foreach ($dependency in @($descriptor.Plugins)) {
	if (-not $dependency) { continue }
	$uplugin = Get-ChildItem -LiteralPath $searchDirs -Recurse -Filter "$($dependency.Name).uplugin" -File -ErrorAction SilentlyContinue | Select-Object -First 1
	if (-not $uplugin) { throw "Plugin dependency $($dependency.Name) not found" }
	$checked++
	if (Test-Experimental $uplugin.FullName) { $violations += "experimental: $($dependency.Name) (listed in $pluginName.uplugin)" }
}

$dependencyLists = 'PublicDependencyModules', 'PrivateDependencyModules', 'PublicIncludePathModules', 'PrivateIncludePathModules', 'DynamicallyLoadedModules'
foreach ($moduleName in $descriptor.Modules.Name) {
	$module = $modules.$moduleName
	if (-not $module) { throw "Module $moduleName is missing from $TargetJson" }
	foreach ($dependencyName in $dependencyLists | ForEach-Object { @($module.$_) } | Where-Object { $_ } | Sort-Object -Unique) {
		$dependency = $modules.$dependencyName
		if (-not $dependency) { throw "Dependency $dependencyName of $moduleName is missing from $TargetJson" }
		$checked++
		$uplugin = Find-OwningPlugin $dependency.Directory
		if ($uplugin -and $uplugin.BaseName -ne $pluginName -and (Test-Experimental $uplugin.FullName)) {
			$violations += "experimental: $($uplugin.BaseName) (module $dependencyName, used by $moduleName)"
		}
	}
}

if ($violations) {
	$violations | ForEach-Object { Write-Host $_ }
	exit 1
}
Write-Host "No experimental engine plugin dependencies ($checked dependencies checked)"
