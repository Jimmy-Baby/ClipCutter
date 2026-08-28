[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PreviousTag,
    [Parameter(Mandatory)][string]$CurrentTag,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$CurrentRef = $CurrentTag,
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')

$repo = Resolve-FullPath $RepositoryRoot
$null = Get-RequestedVersion $CurrentTag
Write-Stage "Generating release notes for $PreviousTag..$CurrentRef as $CurrentTag"
Invoke-Native git @('-C', $repo, 'rev-parse', '--verify', "$PreviousTag^{commit}") | Out-Null
Invoke-Native git @('-C', $repo, 'rev-parse', '--verify', "$CurrentRef^{commit}") | Out-Null
$subjects = @(Invoke-Native git @('-C', $repo, 'log', '--format=%s', "$PreviousTag..$CurrentRef") -CaptureOutput)

$groups = [ordered]@{
    'Features' = [Collections.Generic.List[string]]::new()
    'Fixes' = [Collections.Generic.List[string]]::new()
    'Performance' = [Collections.Generic.List[string]]::new()
    'Documentation' = [Collections.Generic.List[string]]::new()
    'Maintenance' = [Collections.Generic.List[string]]::new()
    'Other changes' = [Collections.Generic.List[string]]::new()
}
foreach ($subjectValue in $subjects) {
    $subject = $subjectValue.ToString().Trim()
    if (-not $subject) { continue }
    $label = 'Other changes'
    $text = $subject
    if ($subject -match '^(?<Type>feat|fix|perf|docs|build|chore|ci|refactor|test)(\([^)]+\))?!?:\s*(?<Text>.+)$') {
        $text = $Matches.Text
        $label = switch ($Matches.Type) {
            'feat' { 'Features' }
            'fix' { 'Fixes' }
            'perf' { 'Performance' }
            'docs' { 'Documentation' }
            default { 'Maintenance' }
        }
    }
    $groups[$label].Add($text)
}

$origin = (Invoke-Native git @('-C', $repo, 'remote', 'get-url', 'origin') -CaptureOutput | Select-Object -First 1).ToString().Trim()
if ($origin -match '^git@github\.com:(?<Repo>.+?)(\.git)?$') { $origin = "https://github.com/$($Matches.Repo)" }
$origin = $origin -replace '\.git$', ''
$comparison = if ($origin -match '^https://github\.com/') { "[$PreviousTag...$CurrentTag]($origin/compare/$PreviousTag...$CurrentTag)" } else { "`$ git diff $PreviousTag..$CurrentTag" }
$lines = [Collections.Generic.List[string]]::new()
$lines.Add("# ClipCutter $CurrentTag")
$lines.Add('')
foreach ($entry in $groups.GetEnumerator()) {
    if ($entry.Value.Count -eq 0) { continue }
    $lines.Add("## $($entry.Key)")
    $lines.Add('')
    foreach ($item in $entry.Value) { $lines.Add("- $item") }
    $lines.Add('')
}
if ($subjects.Count -eq 0) { $lines.Add('- No commit subjects in this range.'); $lines.Add('') }
$lines.Add("**Full comparison:** $comparison")
$output = Resolve-FullPath $OutputPath $repo
[void](New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force)
[IO.File]::WriteAllLines($output, $lines, [Text.UTF8Encoding]::new($false))
Write-Host $output
