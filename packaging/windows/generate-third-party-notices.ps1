[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DependencyManifestPath,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')

$manifest = Get-DependencyManifest (Resolve-FullPath $DependencyManifestPath)
$template = Join-Path (Resolve-FullPath $RepositoryRoot) 'THIRD_PARTY_NOTICES.md'
if (-not (Test-Path -LiteralPath $template -PathType Leaf)) { throw "Notice source missing: $template" }
$content = Get-Content -LiteralPath $template -Raw
foreach ($requiredText in @($manifest.qt.testedVersion, $manifest.ffmpeg.testedVersion,
                             $manifest.qt.license.identifier, $manifest.ffmpeg.license.identifier)) {
    if (-not $content.Contains($requiredText.ToString())) {
        throw "THIRD_PARTY_NOTICES.md is stale; missing dependency-manifest value '$requiredText'."
    }
}
$output = Resolve-FullPath $OutputPath
[void](New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force)
[IO.File]::WriteAllText($output, $content, [Text.UTF8Encoding]::new($false))
Write-Host $output
