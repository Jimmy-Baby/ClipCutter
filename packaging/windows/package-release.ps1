[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$StageDirectory,
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$FfmpegDirectory,
    [Parameter(Mandatory)][string]$QtLicenseFile,
    [Parameter(Mandatory)][string]$FfmpegLicenseFile,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][long]$SourceDateEpoch,
    [string]$WindeployQtPath,
    [string]$QtArchive,
    [string]$FfmpegArchive,
    [string]$DependencyManifestPath = (Resolve-Path (Join-Path $PSScriptRoot '..\dependencies.json')).Path,
    [switch]$OfficialRelease
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$semanticVersion = Get-RequestedVersion $Version
$stage = Resolve-FullPath $StageDirectory
$build = Resolve-FullPath $BuildDirectory
$qt = Resolve-FullPath $QtRoot
$ffmpegDir = Resolve-FullPath $FfmpegDirectory
$output = Resolve-FullPath $OutputDirectory
$qtLicense = Resolve-FullPath $QtLicenseFile
$ffmpegLicense = Resolve-FullPath $FfmpegLicenseFile
$manifestPath = Resolve-FullPath $DependencyManifestPath
$manifest = Get-DependencyManifest $manifestPath
$app = Join-Path $stage 'ClipCutter.exe'
$ffmpeg = Join-Path $ffmpegDir 'ffmpeg.exe'
$ffprobe = Join-Path $ffmpegDir 'ffprobe.exe'
$smokeExecutable = Join-Path $build 'ClipCutterPackageSmoke.exe'
$buildPrefix = $build.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $stage.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Staging must be a child of the declared build directory so cleanup cannot target unrelated files.'
}
if (-not (Test-Path -LiteralPath $app -PathType Leaf)) { throw "Installed application missing: $app" }
foreach ($path in @($ffmpeg, $ffprobe, $qtLicense, $ffmpegLicense, $smokeExecutable)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required packaging input missing: $path" }
}

$deployTool = if ($WindeployQtPath) { Resolve-FullPath $WindeployQtPath } else { Join-Path $qt 'bin\windeployqt.exe' }
$qmake = Join-Path $qt 'bin\qmake.exe'
if (-not (Test-Path -LiteralPath $deployTool -PathType Leaf)) { throw "windeployqt not found: $deployTool" }
if (-not (Test-Path -LiteralPath $qmake -PathType Leaf)) { throw "qmake not found for Qt version validation: $qmake" }

Write-Stage 'Validating dependency versions and provenance'
$qtVersion = (Invoke-Native $qmake @('-query', 'QT_VERSION') -CaptureOutput | Select-Object -First 1).ToString().Trim()
if ($qtVersion -ne $manifest.qt.testedVersion) { throw "Qt version $qtVersion does not match release-tested version $($manifest.qt.testedVersion)." }
$ffmpegLine = Get-ExecutableVersionLine $ffmpeg
$ffprobeLine = Get-ExecutableVersionLine $ffprobe
$ffmpegVersion = Get-VersionFromToolLine $ffmpegLine 'ffmpeg'
$ffprobeVersion = Get-VersionFromToolLine $ffprobeLine 'ffprobe'
if ($ffmpegVersion -ne $manifest.ffmpeg.testedVersion -or $ffprobeVersion -ne $manifest.ffmpeg.ffprobeTestedVersion) {
    throw "FFmpeg/ffprobe version mismatch. Found $ffmpegVersion/$ffprobeVersion; expected $($manifest.ffmpeg.testedVersion)/$($manifest.ffmpeg.ffprobeTestedVersion)."
}
if ($ffmpegVersion -ne $ffprobeVersion) { throw 'FFmpeg and ffprobe versions must match.' }

foreach ($name in @('ffmpeg.exe', 'ffprobe.exe')) {
    $expected = $manifest.ffmpeg.expectedBinarySha256.$name
    if (Test-PopulatedHash $expected) { Assert-FileHash -Path (Join-Path $ffmpegDir $name) -ExpectedSha256 $expected }
}
if ($OfficialRelease) {
    foreach ($dependency in @($manifest.qt, $manifest.ffmpeg)) {
        if ([string]::IsNullOrWhiteSpace($dependency.provenance.sourceUrl) -or -not (Test-PopulatedHash $dependency.provenance.archiveSha256)) {
            throw 'Official release blocked: trusted dependency source URL and verified archive SHA-256 fields are not populated in packaging/dependencies.json.'
        }
    }
    if (-not $QtArchive -or -not $FfmpegArchive) { throw 'Official release requires -QtArchive and -FfmpegArchive for provenance verification.' }
    Assert-FileHash -Path (Resolve-FullPath $QtArchive) -ExpectedSha256 $manifest.qt.provenance.archiveSha256
    Assert-FileHash -Path (Resolve-FullPath $FfmpegArchive) -ExpectedSha256 $manifest.ffmpeg.provenance.archiveSha256
}

Write-Stage 'Deploying Qt runtime into staging'
Invoke-Native $deployTool @('--release', '--no-translations', '--no-system-d3d-compiler',
    '--no-system-dxc-compiler', '--no-compiler-runtime', '--dir', $stage, $app)

Write-Stage 'Adding FFmpeg, ffprobe, licences, and notices'
Copy-Item -LiteralPath $ffmpeg -Destination (Join-Path $stage 'ffmpeg.exe') -Force
Copy-Item -LiteralPath $ffprobe -Destination (Join-Path $stage 'ffprobe.exe') -Force
[void](New-Item -ItemType Directory -Path (Join-Path $stage 'licenses\Qt') -Force)
[void](New-Item -ItemType Directory -Path (Join-Path $stage 'licenses\FFmpeg') -Force)
[void](New-Item -ItemType Directory -Path (Join-Path $stage 'licenses\Qt\SBOM') -Force)
Copy-Item -LiteralPath $qtLicense -Destination (Join-Path $stage 'licenses\Qt\LICENSE.txt') -Force
Copy-Item -LiteralPath $ffmpegLicense -Destination (Join-Path $stage 'licenses\FFmpeg\LICENSE.txt') -Force
foreach ($sbomName in $manifest.qt.sbomFiles) {
    $sbomSource = Join-Path $qt "sbom\$sbomName"
    if (-not (Test-Path -LiteralPath $sbomSource -PathType Leaf)) { throw "Required Qt SBOM missing: $sbomSource" }
    Copy-Item -LiteralPath $sbomSource -Destination (Join-Path $stage "licenses\Qt\SBOM\$sbomName") -Force
}
& (Join-Path $PSScriptRoot 'generate-third-party-notices.ps1') -DependencyManifestPath $manifestPath `
    -OutputPath (Join-Path $stage 'THIRD_PARTY_NOTICES.md') -RepositoryRoot $repo | Out-Null

Write-Stage 'Removing development-only deployment files'
Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object {
    $_.Extension -in @('.pdb', '.lib', '.exp', '.obj', '.ilk') -or $_.Name -match '^ClipCutter.*Tests?\.exe$'
} | Remove-Item -Force
Get-ChildItem -LiteralPath $stage -Recurse -File -Filter '*d.dll' | ForEach-Object {
    $releaseName = $_.BaseName.Substring(0, $_.BaseName.Length - 1) + '.dll'
    if (Test-Path -LiteralPath (Join-Path $_.DirectoryName $releaseName) -PathType Leaf) { Remove-Item -LiteralPath $_.FullName -Force }
}
foreach ($debris in @('CMakeFiles', 'Testing', 'cmake_install.cmake', 'CTestTestfile.cmake', 'CMakeCache.txt', 'build.ninja')) {
    $candidate = Join-Path $stage $debris
    if (Test-Path -LiteralPath $candidate) { Remove-Item -LiteralPath $candidate -Recurse -Force }
}
foreach ($nonBundledRuntime in @('d3dcompiler_47.dll', 'dxcompiler.dll', 'dxil.dll', 'vc_redist.x64.exe')) {
    $candidate = Join-Path $stage $nonBundledRuntime
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { Remove-Item -LiteralPath $candidate -Force }
}
if (Test-Path -LiteralPath (Join-Path $stage 'ClipCutterPackageSmoke.exe')) { throw 'Smoke test executable must not be installed.' }

$qtFfmpegFiles = [Collections.Generic.List[object]]::new()
foreach ($name in $manifest.qt.bundledFfmpegRuntime.expectedFiles) {
    $path = Join-Path $stage $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Qt multimedia FFmpeg runtime missing: $name" }
    $productVersion = (Get-Item -LiteralPath $path).VersionInfo.ProductVersion
    if ($productVersion -ne $manifest.qt.bundledFfmpegRuntime.fileProductVersion) {
        throw "Qt multimedia FFmpeg runtime version mismatch for ${name}: $productVersion"
    }
    $qtFfmpegFiles.Add([ordered]@{
        name = $name
        productVersion = $productVersion
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    })
}

$dependencyEvidence = [ordered]@{
    schemaVersion = 1
    applicationVersion = $semanticVersion
    target = $manifest.target
    sourceDateEpoch = $SourceDateEpoch
    qt = [ordered]@{
        version = $qtVersion
        architecture = $manifest.target.architecture
        deploymentTool = 'windeployqt.exe'
        deploymentToolSha256 = (Get-FileHash -LiteralPath $deployTool -Algorithm SHA256).Hash.ToLowerInvariant()
        bundledFfmpegRuntime = [ordered]@{
            productVersion = $manifest.qt.bundledFfmpegRuntime.fileProductVersion
            licenseIdentifier = $manifest.qt.bundledFfmpegRuntime.licenseIdentifier
            files = @($qtFfmpegFiles)
            attributionLocation = $manifest.qt.bundledFfmpegRuntime.packageAttributionLocation
        }
        sbomFiles = @($manifest.qt.sbomFiles | ForEach-Object { "licenses/Qt/SBOM/$_" })
        provenance = $manifest.qt.provenance
        license = $manifest.qt.license
    }
    ffmpeg = [ordered]@{
        version = $ffmpegVersion
        ffprobeVersion = $ffprobeVersion
        architecture = $manifest.ffmpeg.architecture
        ffmpegVersionLine = $ffmpegLine
        ffprobeVersionLine = $ffprobeLine
        files = @(
            [ordered]@{ name = 'ffmpeg.exe'; sha256 = (Get-FileHash -LiteralPath (Join-Path $stage 'ffmpeg.exe') -Algorithm SHA256).Hash.ToLowerInvariant() },
            [ordered]@{ name = 'ffprobe.exe'; sha256 = (Get-FileHash -LiteralPath (Join-Path $stage 'ffprobe.exe') -Algorithm SHA256).Hash.ToLowerInvariant() }
        )
        provenance = $manifest.ffmpeg.provenance
        license = $manifest.ffmpeg.license
    }
}
[IO.File]::WriteAllText((Join-Path $stage 'DEPENDENCIES.json'), ($dependencyEvidence | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))

Write-Stage 'Checking package text for machine-specific source and build paths'
$forbiddenStrings = @($repo, $build, $qt, $ffmpegDir) | ForEach-Object { $_.ToLowerInvariant() }
foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in @('.conf', '.json', '.ini') }) {
    $content = (Get-Content -LiteralPath $file.FullName -Raw).ToLowerInvariant()
    foreach ($forbidden in $forbiddenStrings) {
        if ($content.Contains($forbidden)) { throw "Machine-specific absolute path found in package file: $($file.FullName)" }
    }
}

& (Join-Path $PSScriptRoot 'smoke-test.ps1') -PackageDirectory $stage -Version $Version `
    -SmokeExecutable $smokeExecutable -DependencyManifestPath $manifestPath -ForbiddenBuildDirectory $build
if ($LASTEXITCODE -ne 0) { throw 'Staged package smoke test failed.' }

Write-Stage 'Generating per-file package manifest'
$fileEntries = [Collections.Generic.List[object]]::new()
Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object Name -ne 'package-manifest.json' | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\', '/')
    $fileEntries.Add([ordered]@{
        path = $relative
        size = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    })
}
$sortedEntries = @($fileEntries | Sort-Object { $_.path })
$packageManifest = [ordered]@{
    schemaVersion = 1
    application = 'ClipCutter'
    version = $semanticVersion
    platform = 'windows-x64'
    sourceDateEpoch = $SourceDateEpoch
    manifestSelfExcluded = $true
    files = $sortedEntries
}
[IO.File]::WriteAllText((Join-Path $stage 'package-manifest.json'), ($packageManifest | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))

Write-Stage 'Creating deterministic archive and SHA-256 checksum'
[void](New-Item -ItemType Directory -Path $output -Force)
$archiveName = "ClipCutter-$Version-windows-x64.zip"
$archive = Join-Path $output $archiveName
& (Join-Path $PSScriptRoot 'new-deterministic-archive.ps1') -PackageDirectory $stage -ArchivePath $archive -SourceDateEpoch $SourceDateEpoch | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Archive creation failed.' }
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
$checksum = "$archiveHash  $archiveName`n"
$checksumPath = "$archive.sha256"
[IO.File]::WriteAllText($checksumPath, $checksum, [Text.UTF8Encoding]::new($false))

Write-Stage 'Extracting archive and repeating essential smoke tests'
$extractedRoot = Reset-Directory -Path (Join-Path $build 'archive-check') -RepositoryRoot $repo
Expand-Archive -LiteralPath $archive -DestinationPath $extractedRoot -Force
& (Join-Path $PSScriptRoot 'smoke-test.ps1') -PackageDirectory $extractedRoot -Version $Version `
    -SmokeExecutable $smokeExecutable -DependencyManifestPath $manifestPath -ForbiddenBuildDirectory $build
if ($LASTEXITCODE -ne 0) { throw 'Extracted package smoke test failed.' }

Write-Host "Package: $archive"
Write-Host "Checksum: $checksumPath"
Write-Host "Staging: $stage"
Write-Host "Extracted verification: $extractedRoot"
