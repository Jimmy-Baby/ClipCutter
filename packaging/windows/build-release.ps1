[CmdletBinding()]
param(
    [string]$Version,
    [string]$PreviousTag,
    [string]$QtRoot,
    [string]$FfmpegDirectory,
    [string]$QtLicenseFile,
    [string]$FfmpegLicenseFile,
    [string]$BuildDirectory,
    [string]$OutputDirectory,
    [ValidateSet('Release')][string]$Configuration = 'Release',
    [string]$WindeployQtPath,
    [string]$QtArchive,
    [string]$FfmpegArchive,
    [Nullable[long]]$SourceDateEpoch,
    [switch]$OfficialRelease,
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')

if ($PSVersionTable.PSVersion.Major -lt 7) { throw 'PowerShell 7 or later is required. Run this script with pwsh.' }
$requiredInputs = [ordered]@{
    Version = $Version
    PreviousTag = $PreviousTag
    QtRoot = $QtRoot
    FfmpegDirectory = $FfmpegDirectory
    QtLicenseFile = $QtLicenseFile
    FfmpegLicenseFile = $FfmpegLicenseFile
}
$missingInputs = @($requiredInputs.GetEnumerator() | Where-Object { [string]::IsNullOrWhiteSpace($_.Value) } | ForEach-Object Key)
if ($missingInputs.Count -gt 0) {
    throw "Missing required parameters: $($missingInputs -join ', '). PowerShell backticks must be the final character on a line; prefer the documented hashtable command."
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'build\windows-package' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'out\release' }
$build = Resolve-FullPath $BuildDirectory $repo
$output = Resolve-FullPath $OutputDirectory $repo
$qt = Resolve-FullPath $QtRoot
$ffmpegDir = Resolve-FullPath $FfmpegDirectory
$manifestPath = Join-Path $repo 'packaging\dependencies.json'

Write-Stage 'Validating version, tag, working tree, tools, and dependency inputs'
$semanticVersion = Assert-VersionAndRepositoryState -RepositoryRoot $repo -Version $Version -OfficialRelease:$OfficialRelease
Enter-ClipCutterMsvcEnvironment
foreach ($tool in @('cmake', 'ninja', 'ctest', 'git')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Required tool not found on PATH: $tool" }
}
foreach ($path in @($qt, $ffmpegDir, (Resolve-FullPath $QtLicenseFile), (Resolve-FullPath $FfmpegLicenseFile))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required input not found: $path" }
}
$manifest = Get-DependencyManifest $manifestPath
$qtConfigDirectory = Join-Path $qt 'lib\cmake\Qt6'
$qtConfig = Join-Path $qtConfigDirectory 'Qt6Config.cmake'
$qmake = Join-Path $qt 'bin\qmake.exe'
if (-not (Test-Path -LiteralPath $qtConfig -PathType Leaf) -or -not (Test-Path -LiteralPath $qmake -PathType Leaf)) {
    throw "QtRoot is not a complete Qt MSVC kit: '$qt'. Expected '$qtConfig' and '$qmake'. Do not escape the underscore in msvc2022_64."
}
$qtVersion = (Invoke-Native $qmake @('-query', 'QT_VERSION') -CaptureOutput | Select-Object -First 1).ToString().Trim()
if ($qtVersion -ne $manifest.qt.testedVersion) {
    throw "QtRoot reports Qt $qtVersion; packaging/dependencies.json requires $($manifest.qt.testedVersion). CMake's 6.5 value is only the minimum supported API version."
}
$ffmpeg = Join-Path $ffmpegDir 'ffmpeg.exe'
$ffprobe = Join-Path $ffmpegDir 'ffprobe.exe'
$ffmpegVersion = Get-VersionFromToolLine (Get-ExecutableVersionLine $ffmpeg) 'ffmpeg'
$ffprobeVersion = Get-VersionFromToolLine (Get-ExecutableVersionLine $ffprobe) 'ffprobe'
if ($ffmpegVersion -ne $manifest.ffmpeg.testedVersion -or $ffprobeVersion -ne $manifest.ffmpeg.ffprobeTestedVersion) {
    throw "Dependency versions do not match packaging/dependencies.json: $ffmpegVersion/$ffprobeVersion."
}
if ($ValidateOnly) {
    Write-Host "Validation passed for ClipCutter $Version (Qt $($manifest.qt.testedVersion), FFmpeg $ffmpegVersion, ffprobe $ffprobeVersion)."
    return
}

if ($null -eq $SourceDateEpoch) {
    $epochText = (Invoke-Native git @('-C', $repo, 'show', '-s', '--format=%ct', 'HEAD') -CaptureOutput | Select-Object -First 1).ToString().Trim()
    $SourceDateEpoch = [long]$epochText
}
$sourceEpoch = [long]$SourceDateEpoch
if ($sourceEpoch -lt 315532800) { throw 'SOURCE_DATE_EPOCH must be 1980-01-01 or later for ZIP compatibility.' }

Write-Stage 'Creating clean Release build and staging directories'
$build = Reset-Directory -Path $build -RepositoryRoot $repo
$stage = Reset-Directory -Path (Join-Path $build 'stage') -RepositoryRoot $repo

Write-Stage 'Configuring Release build'
$configureArguments = @(
    '-S', $repo,
    '-B', $build,
    '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration",
    '-DBUILD_TESTING=ON',
    '-DCLIPCUTTER_BUILD_PACKAGING_TOOLS=ON',
    "-DCMAKE_PREFIX_PATH=$qt",
    "-DQt6_DIR=$qtConfigDirectory",
    "-DCMAKE_INSTALL_PREFIX=$stage"
)
Invoke-Native cmake $configureArguments

Write-Stage 'Building ClipCutter and packaging diagnostics'
Invoke-Native cmake @('--build', $build, '--config', $Configuration, '--parallel')

Write-Stage 'Running unit and FFmpeg integration tests'
$savedFfmpeg = $env:CLIPCUTTER_TEST_FFMPEG
$savedFfprobe = $env:CLIPCUTTER_TEST_FFPROBE
try {
    $env:CLIPCUTTER_TEST_FFMPEG = $ffmpeg
    $env:CLIPCUTTER_TEST_FFPROBE = $ffprobe
    Invoke-Native ctest @('--test-dir', $build, '--build-config', $Configuration, '--output-on-failure')
} finally {
    if ($null -eq $savedFfmpeg) { Remove-Item Env:CLIPCUTTER_TEST_FFMPEG -ErrorAction SilentlyContinue } else { $env:CLIPCUTTER_TEST_FFMPEG = $savedFfmpeg }
    if ($null -eq $savedFfprobe) { Remove-Item Env:CLIPCUTTER_TEST_FFPROBE -ErrorAction SilentlyContinue } else { $env:CLIPCUTTER_TEST_FFPROBE = $savedFfprobe }
}

Write-Stage 'Installing project-owned runtime files into fresh staging'
Invoke-Native cmake @('--install', $build, '--config', $Configuration, '--prefix', $stage)

Write-Stage 'Generating editable local release notes'
[void](New-Item -ItemType Directory -Path $output -Force)
$notes = Join-Path $output "release-notes-$Version.md"
$currentRef = if ($OfficialRelease) { $Version } else { 'HEAD' }
& (Join-Path $PSScriptRoot 'generate-release-notes.ps1') -PreviousTag $PreviousTag -CurrentTag $Version `
    -CurrentRef $currentRef -OutputPath $notes -RepositoryRoot $repo

Write-Stage 'Deploying, smoke testing, archiving, extracting, and re-testing package'
$packageArguments = @{
    Version = $Version
    StageDirectory = $stage
    BuildDirectory = $build
    QtRoot = $qt
    FfmpegDirectory = $ffmpegDir
    QtLicenseFile = (Resolve-FullPath $QtLicenseFile)
    FfmpegLicenseFile = (Resolve-FullPath $FfmpegLicenseFile)
    OutputDirectory = $output
    SourceDateEpoch = $sourceEpoch
    DependencyManifestPath = $manifestPath
    OfficialRelease = $OfficialRelease
}
if ($WindeployQtPath) { $packageArguments.WindeployQtPath = $WindeployQtPath }
if ($QtArchive) { $packageArguments.QtArchive = $QtArchive }
if ($FfmpegArchive) { $packageArguments.FfmpegArchive = $FfmpegArchive }
& (Join-Path $PSScriptRoot 'package-release.ps1') @packageArguments

Write-Host "Release notes: $notes"
Write-Host "Archive: $(Join-Path $output "ClipCutter-$Version-windows-x64.zip")"
Write-Host "Checksum: $(Join-Path $output "ClipCutter-$Version-windows-x64.zip.sha256")"
