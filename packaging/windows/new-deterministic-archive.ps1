[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$ArchivePath,
    [Parameter(Mandatory)][long]$SourceDateEpoch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')

$package = Resolve-FullPath $PackageDirectory
$archive = Resolve-FullPath $ArchivePath
if (-not (Test-Path -LiteralPath $package -PathType Container)) { throw "Package directory not found: $package" }
if ($archive.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Archive must not be created inside the package directory.'
}
$timestamp = [DateTimeOffset]::FromUnixTimeSeconds($SourceDateEpoch)
$minimum = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
$maximum = [DateTimeOffset]::new(2107, 12, 31, 23, 59, 58, [TimeSpan]::Zero)
if ($timestamp -lt $minimum -or $timestamp -gt $maximum) { throw 'SOURCE_DATE_EPOCH must map to a ZIP timestamp between 1980 and 2107.' }
$timestamp = $timestamp.AddTicks(-($timestamp.Ticks % ([TimeSpan]::TicksPerSecond * 2)))

$parent = Split-Path -Parent $archive
[void](New-Item -ItemType Directory -Path $parent -Force)
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$files = [Collections.Generic.List[string]]::new()
Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($package, $_.FullName).Replace('\', '/')
    $files.Add($relative)
}
$files.Sort([StringComparer]::Ordinal)

$stream = [IO.File]::Open($archive, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false, [Text.Encoding]::UTF8)
    try {
        foreach ($relative in $files) {
            $source = Join-Path $package ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar))
            $entry = $zip.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $timestamp
            $input = [IO.File]::OpenRead($source)
            $output = $entry.Open()
            try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }

Write-Host $archive
