param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [ValidateSet("development", "preview", "stable")]
    [string]$Channel,

    [Parameter(Mandatory = $true)]
    [ValidateSet("x86_64", "x86")]
    [string]$Architecture,

    [Parameter(Mandatory = $true)]
    [ValidateSet("x64", "Win32")]
    [string]$Platform,

    [Parameter(Mandatory = $true)]
    [string]$BinDir,

    [Parameter(Mandatory = $true)]
    [string]$DataDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [Parameter(Mandatory = $true)]
    [string]$SourceCommit,

    [Parameter(Mandatory = $true)]
    [string]$Repository,

    [string]$NotesUrl = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$') {
    throw "Version '$Version' is not a supported semantic version."
}

if ($SourceCommit -notmatch '^[0-9a-f]{40}$') {
    throw "SourceCommit must be a lowercase 40-character Git commit SHA."
}

$binPath = (Resolve-Path $BinDir).Path
$dataPath = (Resolve-Path $DataDir).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$outputPath = (Resolve-Path $OutputDir).Path

$required = @(
    @{ Name = "NocturneEditor.exe"; Kind = "bin" },
    @{ Name = "NocturneHost.exe"; Kind = "bin" },
    @{ Name = "NocturneEngine.lib"; Kind = "lib" }
)

foreach ($entry in $required) {
    $candidate = Join-Path $binPath $entry.Name
    if (-not (Test-Path $candidate -PathType Leaf)) {
        throw "Required Ship output missing: $candidate"
    }
}

$artifactName = "nocturne-engine-$Version-windows-$Architecture.zip"
$stagePath = Join-Path $outputPath "stage-$Architecture"
$archivePath = Join-Path $outputPath $artifactName
$recordPath = Join-Path $outputPath "release-record-$Architecture.json"
$checksumPath = "$archivePath.sha256"

if (Test-Path $stagePath) {
    Remove-Item -Recurse -Force $stagePath
}
if (Test-Path $archivePath) {
    Remove-Item -Force $archivePath
}
if (Test-Path $checksumPath) {
    Remove-Item -Force $checksumPath
}

$stageBin = Join-Path $stagePath "bin"
$stageLib = Join-Path $stagePath "lib"
$stageData = Join-Path $stagePath "Data"

New-Item -ItemType Directory -Force -Path $stageBin | Out-Null
New-Item -ItemType Directory -Force -Path $stageLib | Out-Null

Copy-Item (Join-Path $binPath "NocturneEditor.exe") $stageBin
Copy-Item (Join-Path $binPath "NocturneHost.exe") $stageBin
Copy-Item (Join-Path $binPath "NocturneEngine.lib") $stageLib
Copy-Item $dataPath $stageData -Recurse

$releaseInfo = [ordered]@{
    product = "Nocturne Engine"
    version = $Version
    channel = $Channel
    platform = "windows"
    architecture = $Architecture
    msbuildPlatform = $Platform
    configuration = "ship"
    sourceCommit = $SourceCommit
}

$releaseInfoJson = $releaseInfo | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText(
    (Join-Path $stagePath "release-info.json"),
    $releaseInfoJson,
    (New-Object System.Text.UTF8Encoding($false))
)

Compress-Archive -Path (Join-Path $stagePath "*") -DestinationPath $archivePath -CompressionLevel Optimal

$archive = Get-Item $archivePath
$sha256 = (Get-FileHash -Algorithm SHA256 $archivePath).Hash.ToLowerInvariant()
"$sha256  $artifactName" | Set-Content -Encoding ASCII $checksumPath

if ([string]::IsNullOrWhiteSpace($NotesUrl)) {
    $NotesUrl = "https://github.com/$Repository/releases/tag/v$Version"
}

$downloadUrl = "https://github.com/$Repository/releases/download/v$Version/$artifactName"

$releaseRecord = [ordered]@{
    version = $Version
    channel = $Channel
    publishedAt = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd")
    notesUrl = $NotesUrl
    sourceCommit = $SourceCommit
    builds = @(
        [ordered]@{
            platform = "windows"
            architecture = $Architecture
            configuration = "ship"
            status = "supported"
            file = $artifactName
            bytes = [int64]$archive.Length
            sha256 = $sha256
            url = $downloadUrl
        }
    )
}

$releaseRecordJson = $releaseRecord | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText(
    $recordPath,
    $releaseRecordJson,
    (New-Object System.Text.UTF8Encoding($false))
)

if ($env:GITHUB_OUTPUT) {
    "artifact_path=$archivePath" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "artifact_name=$artifactName" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "checksum_path=$checksumPath" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "record_path=$recordPath" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "sha256=$sha256" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "bytes=$($archive.Length)" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
}

Write-Host "Packaged $artifactName"
Write-Host "SHA-256: $sha256"
Write-Host "Bytes: $($archive.Length)"
