<#
Creates the content-only Unreal 5.8 base project that hosts this plugin for building and
testing, and links this repo into it as Plugins\Cambriana (a directory junction).
Safe to rerun: an existing project file or matching junction is left as is.
#>
param(
	[string]$ProjectDir = 'D:\3\ue\5.8\dev\Cambriana_Base'
)

$ErrorActionPreference = 'Stop'

$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectName = Split-Path $ProjectDir -Leaf
$projectFile = Join-Path $ProjectDir "$projectName.uproject"
$pluginLink = Join-Path $ProjectDir 'Plugins\Cambriana'

New-Item -ItemType Directory -Force -Path (Join-Path $ProjectDir 'Plugins') | Out-Null

if (-not (Test-Path $projectFile)) {
	$descriptor = [ordered]@{
		FileVersion       = 3
		EngineAssociation = '5.8'
		Category          = ''
		Description       = 'Base project hosting the Cambriana plugin for building and testing.'
		Plugins           = @([ordered]@{ Name = 'Cambriana'; Enabled = $true })
	}
	$descriptor | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 -Path $projectFile
	Write-Host "Created $projectFile"
}

if (Test-Path $pluginLink) {
	$item = Get-Item $pluginLink -Force
	$target = @($item.Target)[0]
	if ($item.LinkType -ne 'Junction' -or (Resolve-Path $target).Path -ne $repoDir) {
		throw "$pluginLink exists and is not a junction to $repoDir"
	}
	Write-Host "$pluginLink already links to $repoDir"
}
else {
	New-Item -ItemType Junction -Path $pluginLink -Target $repoDir | Out-Null
	Write-Host "Linked $pluginLink -> $repoDir"
}
