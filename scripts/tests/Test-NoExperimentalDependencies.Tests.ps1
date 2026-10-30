<#
Runs scripts\Test-NoExperimentalDependencies.ps1 against fixture engines, projects and target
exports. Exits non-zero if any case fails.
#>
$ErrorActionPreference = 'Stop'

$check = Join-Path $PSScriptRoot '..\Test-NoExperimentalDependencies.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) "Cambriana-ats05-$PID"
$engine = Join-Path $root 'Engine'
$project = Join-Path $root 'Project'
$pluginDir = Join-Path $project 'Plugins\Cambriana'

function New-File([string]$Path, [string]$Text) {
	New-Item -ItemType Directory -Force -Path (Split-Path $Path) | Out-Null
	Set-Content -LiteralPath $Path -Value $Text -Encoding utf8
}

function New-EnginePlugin([string]$RelativeDir, [string]$Name, [string]$Flags) {
	$dir = Join-Path $engine "Engine\Plugins\$RelativeDir\$Name"
	New-File (Join-Path $dir "$Name.uplugin") "{ `"FileVersion`": 3, $Flags }"
	New-Item -ItemType Directory -Force -Path (Join-Path $dir "Source\$($Name)Module") | Out-Null
	Join-Path $dir "Source\$($Name)Module"
}

function Invoke-Case([string]$Name, [string[]]$ModuleDependencies, [string[]]$PluginDependencies, [int]$ExpectedExit, [string]$ExpectedOutput) {
	$plugins = ($PluginDependencies | ForEach-Object { "{ `"Name`": `"$_`", `"Enabled`": true }" }) -join ', '
	New-File (Join-Path $pluginDir 'Cambriana.uplugin') "{ `"FileVersion`": 3, `"Modules`": [ { `"Name`": `"Cambriana`" } ], `"Plugins`": [ $plugins ] }"
	$targetModules = [ordered]@{
		Cambriana = [ordered]@{ Directory = (Join-Path $pluginDir 'Source\Cambriana'); PrivateDependencyModules = @($ModuleDependencies) }
	}
	foreach ($dependency in $ModuleDependencies) { $targetModules[$dependency] = @{ Directory = $moduleDirs[$dependency] } }
	$targetJson = Join-Path $root 'target.json'
	@{ Modules = $targetModules } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $targetJson -Encoding utf8

	$output = (& powershell -NoProfile -File $check -ProjectFile (Join-Path $project 'Project.uproject') -EngineDir $engine -PluginDescriptor (Join-Path $pluginDir 'Cambriana.uplugin') -TargetJson $targetJson | Out-String)
	$exit = $LASTEXITCODE
	if ($exit -eq $ExpectedExit -and $output -match $ExpectedOutput) {
		Write-Host "PASS $Name"
		return $true
	}
	Write-Host "FAIL $Name (exit $exit, expected $ExpectedExit)`n$output"
	$false
}

try {
	New-File (Join-Path $project 'Project.uproject') '{ "FileVersion": 3 }'
	New-Item -ItemType Directory -Force -Path (Join-Path $pluginDir 'Source\Cambriana') | Out-Null
	$coreDir = Join-Path $engine 'Engine\Source\Runtime\Core'
	New-Item -ItemType Directory -Force -Path $coreDir | Out-Null
	$moduleDirs = @{
		Core               = $coreDir
		StableModule       = New-EnginePlugin 'Runtime' 'Stable' '"IsExperimentalVersion": false'
		BetaInFolderModule = New-EnginePlugin 'Experimental' 'BetaInFolder' '"IsBetaVersion": true,'
		FlaggedModule      = New-EnginePlugin 'Runtime' 'Flagged' '"IsExperimentalVersion" : true,'
	}

	$results = @(
		Invoke-Case 'EngineModulesAndStablePluginsPass' @('Core', 'StableModule') @('Stable') 0 'No experimental engine plugin dependencies \(3 dependencies checked\)'
		Invoke-Case 'ExperimentalFolderWithoutFlagPasses' @('BetaInFolderModule') @('BetaInFolder') 0 'No experimental'
		Invoke-Case 'ModuleInFlaggedPluginFails' @('Core', 'FlaggedModule') @() 1 'experimental: Flagged \(module FlaggedModule, used by Cambriana\)'
		Invoke-Case 'FlaggedPluginInDescriptorFails' @('Core') @('Flagged') 1 'experimental: Flagged \(listed in Cambriana.uplugin\)'
	)
}
finally {
	Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}

if ($results -contains $false) { exit 1 }
