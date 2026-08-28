Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Stage {
    param([Parameter(Mandatory)][string]$Message)
    Write-Host "==> $Message"
}

function Enter-ClipCutterMsvcEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    $programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'MSVC is not active and vswhere.exe was not found. Install Visual Studio 2022 C++ Build Tools.'
    }
    $installation = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) |
        Select-Object -First 1
    if (-not $installation) { throw 'No Visual Studio installation with x64 C++ tools was found.' }
    $installationPath = $installation.ToString().Trim()
    $devShellModule = Join-Path $installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
        throw "Visual Studio developer-shell module not found: $devShellModule"
    }
    Write-Stage 'Initializing Visual Studio x64 developer environment'
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $installationPath -SkipAutomaticLocation `
        -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'Visual Studio developer environment initialized, but cl.exe remains unavailable.'
    }
}

function Invoke-Native {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter()][string[]]$ArgumentList = @(),
        [switch]$CaptureOutput
    )
    Write-Verbose ("{0} {1}" -f $FilePath, ($ArgumentList -join ' '))
    if ($CaptureOutput) {
        $result = & $FilePath @ArgumentList 2>&1
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) { throw "Command failed with exit code ${exitCode}: $FilePath`n$($result -join [Environment]::NewLine)" }
        return $result
    }
    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE`: $FilePath" }
}

function Resolve-FullPath {
    param([Parameter(Mandatory)][string]$Path, [string]$BasePath = (Get-Location).Path)
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

function Assert-SafeRecreatePath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$RepositoryRoot)
    $full = Resolve-FullPath $Path
    $root = [IO.Path]::GetPathRoot($full)
    $repo = Resolve-FullPath $RepositoryRoot
    $separator = [IO.Path]::DirectorySeparatorChar
    $isRepositoryAncestor = $repo.StartsWith($full.TrimEnd($separator) + $separator, [StringComparison]::OrdinalIgnoreCase)
    $safeLeaf = [IO.Path]::GetFileName($full.TrimEnd($separator)) -match '(?i)(build|package|stage|archive|extract|release|dist|out|temp|tmp)'
    if ([string]::IsNullOrWhiteSpace($full) -or $full -eq $root -or $full -eq $repo -or $full.Length -lt 8 -or
        $isRepositoryAncestor -or -not $safeLeaf) {
        throw "Refusing unsafe directory recreation target: '$full'"
    }
    return $full
}

function Reset-Directory {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$RepositoryRoot)
    $full = Assert-SafeRecreatePath -Path $Path -RepositoryRoot $RepositoryRoot
    if (Test-Path -LiteralPath $full) { Remove-Item -LiteralPath $full -Recurse -Force }
    [void](New-Item -ItemType Directory -Path $full -Force)
    return $full
}

function Get-RequestedVersion {
    param([Parameter(Mandatory)][string]$Version)
    if ($Version -notmatch '^v(?<SemVer>(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*))$') {
        throw "Version must use the exact vX.Y.Z form: '$Version'"
    }
    return $Matches.SemVer
}

function Get-CMakeProjectVersion {
    param([Parameter(Mandatory)][string]$RepositoryRoot)
    $cmake = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'CMakeLists.txt') -Raw
    if ($cmake -notmatch 'project\s*\(\s*ClipCutter\s+VERSION\s+(?<Version>\d+\.\d+\.\d+)') {
        throw 'Could not read the authoritative ClipCutter project version from CMakeLists.txt.'
    }
    return $Matches.Version
}

function Assert-VersionAndRepositoryState {
    param(
        [Parameter(Mandatory)][string]$RepositoryRoot,
        [Parameter(Mandatory)][string]$Version,
        [switch]$OfficialRelease
    )
    $semanticVersion = Get-RequestedVersion $Version
    $projectVersion = Get-CMakeProjectVersion $RepositoryRoot
    if ($semanticVersion -ne $projectVersion) {
        throw "Requested version $Version does not match CMake project version $projectVersion. Update only project(... VERSION ...) first."
    }
    $inside = (Invoke-Native git @('-C', $RepositoryRoot, 'rev-parse', '--is-inside-work-tree') -CaptureOutput | Select-Object -First 1).ToString().Trim()
    if ($inside -ne 'true') { throw 'Release source is not a Git working tree.' }
    $exactTag = $null
    $tagOutput = & git -C $RepositoryRoot describe --tags --exact-match HEAD 2>$null
    if ($LASTEXITCODE -eq 0) { $exactTag = ($tagOutput | Select-Object -First 1).ToString().Trim() }
    if ($exactTag -and $exactTag -ne $Version) { throw "HEAD tag '$exactTag' does not match requested release tag '$Version'." }
    $dirty = @(& git -C $RepositoryRoot status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect Git working-tree state.' }
    if ($dirty.Count -gt 0) {
        if ($OfficialRelease) { throw 'Official releases require a clean working tree.' }
        Write-Warning 'Working tree is dirty; output is a local validation package, not an official release.'
    }
    if ($OfficialRelease -and $exactTag -ne $Version) {
        throw "Official releases require HEAD to be exactly tagged '$Version'. Branch names are never used as versions."
    }
    return $semanticVersion
}

function Get-DependencyManifest {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Dependency manifest not found: $Path" }
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-ExecutableVersionLine {
    param([Parameter(Mandatory)][string]$Executable)
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Executable not found: $Executable" }
    $output = Invoke-Native $Executable @('-version') -CaptureOutput
    return ($output | Select-Object -First 1).ToString().Trim()
}

function Get-VersionFromToolLine {
    param([Parameter(Mandatory)][string]$Line, [Parameter(Mandatory)][string]$ToolName)
    if ($Line -notmatch ('^' + [regex]::Escape($ToolName) + ' version (?<Version>\d+\.\d+\.\d+)')) {
        throw "Could not parse $ToolName semantic version from: $Line"
    }
    return $Matches.Version
}

function Assert-FileHash {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$ExpectedSha256)
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $ExpectedSha256.ToLowerInvariant()) { throw "SHA-256 mismatch for '$Path'. Expected $ExpectedSha256; got $actual." }
}

function Test-PopulatedHash {
    param($Value)
    return $null -ne $Value -and $Value.ToString() -match '^[0-9a-fA-F]{64}$'
}
