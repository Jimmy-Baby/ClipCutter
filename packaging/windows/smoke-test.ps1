[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$SmokeExecutable,
    [string]$DependencyManifestPath = (Resolve-Path (Join-Path $PSScriptRoot '..\dependencies.json')).Path,
    [string]$ForbiddenBuildDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')

$semanticVersion = Get-RequestedVersion $Version
$package = Resolve-FullPath $PackageDirectory
$smokeSource = Resolve-FullPath $SmokeExecutable
$manifest = Get-DependencyManifest $DependencyManifestPath
if (-not (Test-Path -LiteralPath $package -PathType Container)) { throw "Package directory not found: $package" }
if (-not (Test-Path -LiteralPath $smokeSource -PathType Leaf)) { throw "Smoke executable not found: $smokeSource" }

function Invoke-PackagedApplication {
    param([Parameter(Mandatory)][string[]]$ArgumentList)
    $standardOutput = [IO.Path]::GetTempFileName()
    $standardError = [IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath (Join-Path $package 'ClipCutter.exe') -ArgumentList $ArgumentList `
            -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $standardOutput -RedirectStandardError $standardError
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            StandardOutput = (Get-Content -LiteralPath $standardOutput -Raw)
            StandardError = (Get-Content -LiteralPath $standardError -Raw)
        }
    } finally {
        Remove-Item -LiteralPath $standardOutput, $standardError -Force -ErrorAction SilentlyContinue
    }
}

Write-Stage "Checking staged package structure"
$required = @('ClipCutter.exe', 'ffmpeg.exe', 'ffprobe.exe') + @($manifest.qt.expectedFiles)
foreach ($relative in $required) {
    $candidate = Join-Path $package ($relative.ToString().Replace('/', [IO.Path]::DirectorySeparatorChar))
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "Required package file missing: $relative" }
}
foreach ($alternatives in $manifest.qt.expectedAnyOf) {
    $found = $false
    foreach ($relative in $alternatives) {
        $candidate = Join-Path $package ($relative.ToString().Replace('/', [IO.Path]::DirectorySeparatorChar))
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $found = $true }
    }
    if (-not $found) { throw "Required multimedia plugin missing; expected one of: $($alternatives -join ', ')" }
}

$savedPath = $env:Path
$savedPluginPath = $env:QT_PLUGIN_PATH
$savedQmlPath = $env:QML2_IMPORT_PATH
$savedQtPlatform = $env:QT_QPA_PLATFORM
try {
    $env:Path = "$package;$env:SystemRoot\System32;$env:SystemRoot"
    Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QML2_IMPORT_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue

    Write-Stage "Checking packaged application version and runtime origins"
    $versionResult = Invoke-PackagedApplication @('--version')
    if ($versionResult.ExitCode -ne 0) { throw "Packaged ClipCutter --version failed: $($versionResult.StandardError)" }
    $versionText = $versionResult.StandardOutput.Trim()
    if ($versionText -notmatch ('(^|\s)' + [regex]::Escape($semanticVersion) + '($|\s)')) {
        throw "Packaged application version mismatch. Expected $semanticVersion; got '$versionText'."
    }

    $diagnosticResult = Invoke-PackagedApplication @('--runtime-diagnostics')
    if ($diagnosticResult.ExitCode -ne 0) { throw "Packaged runtime diagnostics failed: $($diagnosticResult.StandardError)" }
    $diagnostics = $diagnosticResult.StandardOutput | ConvertFrom-Json
    if ($diagnostics.applicationVersion -ne $semanticVersion) { throw 'Runtime diagnostic application version mismatch.' }
    foreach ($tool in @('ffmpegPath', 'ffprobePath')) {
        $resolved = Resolve-FullPath $diagnostics.$tool
        if (-not $resolved.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "$tool resolved outside the package: $resolved"
        }
    }
    $qtModules = @($diagnostics.loadedModules | Where-Object { [IO.Path]::GetFileName($_) -match '^Qt6.*\.dll$' })
    if ($qtModules.Count -eq 0) { throw 'Runtime diagnostics did not report any loaded Qt DLLs.' }
    foreach ($module in $qtModules) {
        $resolved = Resolve-FullPath $module
        if (-not $resolved.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Qt dependency loaded outside the package: $resolved"
        }
    }
    if ($ForbiddenBuildDirectory) {
        $forbidden = (Resolve-FullPath $ForbiddenBuildDirectory).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        foreach ($module in @($diagnostics.loadedModules)) {
            $resolved = Resolve-FullPath $module
            $insidePackage = $resolved.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
            if (-not $insidePackage -and $resolved.StartsWith($forbidden, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Dependency loaded from the developer build directory: $module"
            }
        }
    }

    Write-Stage "Running media probe and core export smoke test"
    $injectedSmoke = Join-Path $package 'ClipCutterPackageSmoke.exe'
    if (Test-Path -LiteralPath $injectedSmoke) { throw "Package unexpectedly contains smoke executable: $injectedSmoke" }
    Copy-Item -LiteralPath $smokeSource -Destination $injectedSmoke
    try {
        $smokeOutput = @(& $injectedSmoke $package 2>&1)
        if ($LASTEXITCODE -ne 0) { throw "Core package smoke test failed:`n$($smokeOutput -join [Environment]::NewLine)" }
    } finally {
        if (Test-Path -LiteralPath $injectedSmoke) { Remove-Item -LiteralPath $injectedSmoke -Force }
    }

    $packagedManifestPath = Join-Path $package 'package-manifest.json'
    if (Test-Path -LiteralPath $packagedManifestPath -PathType Leaf) {
        Write-Stage "Verifying packaged per-file hashes"
        $packagedManifest = Get-Content -LiteralPath $packagedManifestPath -Raw | ConvertFrom-Json
        foreach ($entry in $packagedManifest.files) {
            $file = Join-Path $package ($entry.path.ToString().Replace('/', [IO.Path]::DirectorySeparatorChar))
            if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Manifest file missing: $($entry.path)" }
            if ((Get-Item -LiteralPath $file).Length -ne [long]$entry.size) { throw "Manifest size mismatch: $($entry.path)" }
            Assert-FileHash -Path $file -ExpectedSha256 $entry.sha256
        }
    }
} finally {
    $env:Path = $savedPath
    if ($null -eq $savedPluginPath) { Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue } else { $env:QT_PLUGIN_PATH = $savedPluginPath }
    if ($null -eq $savedQmlPath) { Remove-Item Env:QML2_IMPORT_PATH -ErrorAction SilentlyContinue } else { $env:QML2_IMPORT_PATH = $savedQmlPath }
    if ($null -eq $savedQtPlatform) { Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue } else { $env:QT_QPA_PLATFORM = $savedQtPlatform }
}

Write-Host "Smoke test passed: $package"
