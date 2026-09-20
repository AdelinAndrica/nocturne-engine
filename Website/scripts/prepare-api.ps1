param(
    [string]$DoxygenExecutable = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$websiteRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$repoRoot = (Resolve-Path (Join-Path $websiteRoot "..")).Path

function Resolve-Doxygen {
    param([string]$Explicit)

    if (-not [string]::IsNullOrWhiteSpace($Explicit)) {
        if (-not (Test-Path -LiteralPath $Explicit -PathType Leaf)) {
            throw "Doxygen executable not found: $Explicit"
        }
        return (Resolve-Path $Explicit).Path
    }

    $command = Get-Command doxygen -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "C:\Program Files\doxygen\bin\doxygen.exe",
        "C:\Program Files (x86)\doxygen\bin\doxygen.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw "Doxygen was not found. Install Doxygen or pass -DoxygenExecutable."
}

function Get-SymbolAlias {
    param([string]$QualifiedName)
    return ($QualifiedName -replace "::", ".")
}

$doxygen = Resolve-Doxygen $DoxygenExecutable
$doxygenVersion = (& $doxygen --version).Trim()

$buildRoot = Join-Path $repoRoot "Build\Docs\Doxygen"
$htmlSource = Join-Path $buildRoot "html"
$xmlSource = Join-Path $buildRoot "xml"
$tagSource = Join-Path $buildRoot "nocturne.tag"

$publicApi = Join-Path $websiteRoot "public\api"
$publicXml = Join-Path $websiteRoot "public\api-xml"
$publicSymbol = Join-Path $websiteRoot "public\api-symbol"
$symbolIndexPath = Join-Path $websiteRoot "public\api-symbols.json"
$buildInfoPath = Join-Path $websiteRoot "public\api-build.json"

foreach ($path in @($buildRoot, $publicApi, $publicXml, $publicSymbol)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

foreach ($file in @($symbolIndexPath, $buildInfoPath)) {
    if (Test-Path -LiteralPath $file) {
        Remove-Item -LiteralPath $file -Force
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $buildRoot) | Out-Null
New-Item -ItemType Directory -Force -Path $publicApi | Out-Null
New-Item -ItemType Directory -Force -Path $publicXml | Out-Null
New-Item -ItemType Directory -Force -Path $publicSymbol | Out-Null

Push-Location $repoRoot
try {
    & $doxygen "Docs/API/Doxyfile"
    if ($LASTEXITCODE -ne 0) {
        throw "Doxygen failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$htmlIndex = Join-Path $htmlSource "index.html"
$xmlIndex = Join-Path $xmlSource "index.xml"

if (-not (Test-Path -LiteralPath $htmlIndex -PathType Leaf)) {
    throw "Doxygen HTML index missing: $htmlIndex"
}
if (-not (Test-Path -LiteralPath $xmlIndex -PathType Leaf)) {
    throw "Doxygen XML index missing: $xmlIndex"
}
if (-not (Test-Path -LiteralPath $tagSource -PathType Leaf)) {
    throw "Doxygen tag file missing: $tagSource"
}

Copy-Item -Path (Join-Path $htmlSource "*") -Destination $publicApi -Recurse -Force
Copy-Item -Path (Join-Path $xmlSource "*") -Destination $publicXml -Recurse -Force
Copy-Item -LiteralPath $tagSource -Destination (Join-Path $publicApi "nocturne.tag") -Force

[xml]$xml = Get-Content -LiteralPath $xmlIndex -Raw

$compounds = @()
foreach ($compound in $xml.doxygenindex.compound) {
    $kind = [string]$compound.kind
    $name = [string]$compound.name
    $refid = [string]$compound.refid

    if ($kind -notin @("class", "struct", "union", "namespace")) {
        continue
    }
    if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($refid)) {
        continue
    }

    $alias = Get-SymbolAlias $name
    $target = "/api/$refid.html"
    $aliasFile = Join-Path $publicSymbol "$alias.html"

    $escapedName = [System.Net.WebUtility]::HtmlEncode($name)
    $escapedTarget = [System.Net.WebUtility]::HtmlEncode($target)

    $redirect = @"
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="0; url=$escapedTarget">
  <link rel="canonical" href="$escapedTarget">
  <title>$escapedName — Nocturne C++ API</title>
</head>
<body>
  <p>Redirecting to <a href="$escapedTarget">$escapedName</a>.</p>
</body>
</html>
"@

    [System.IO.File]::WriteAllText(
        $aliasFile,
        $redirect,
        (New-Object System.Text.UTF8Encoding($false))
    )

    $compounds += [ordered]@{
        name = $name
        kind = $kind
        refid = $refid
        href = "/api-symbol/$alias.html"
        doxygenHref = $target
    }
}

$compounds = @($compounds | Sort-Object name, kind)

$requiredSymbols = @(
    "noc::Engine",
    "noc::MainLoop",
    "noc::VirtualFileSystem",
    "noc::ResourceManager",
    "noc::RenderSystem",
    "noc::RenderQueue",
    "noc::World",
    "noc::EntityHandle",
    "noc::EntityRegistry",
    "nocturne::editor::EditorShellV3",
    "nocturne::editor::EditorViewportController",
    "nocturne::editor::EditorTheme"
)

$symbolNames = @($compounds | ForEach-Object { $_.name })
foreach ($required in $requiredSymbols) {
    if ($required -notin $symbolNames) {
        throw "Required API symbol missing from Doxygen XML: $required"
    }
}

$symbolIndex = [ordered]@{
    schemaVersion = 1
    generator = "doxygen"
    doxygenVersion = $doxygenVersion
    compounds = $compounds
}

$symbolIndex |
    ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $symbolIndexPath -Encoding UTF8

$headerRoots = @(
    (Join-Path $repoRoot "Engine"),
    (Join-Path $repoRoot "Apps\NocturneEditor"),
    (Join-Path $repoRoot "Apps\NocturneHost")
)

$headerCount = @(
    Get-ChildItem -Path $headerRoots -Recurse -File |
        Where-Object {
            $_.Extension -in @(".h", ".hpp", ".inl") -and
            $_.FullName -notlike "*\Tests\*" -and
            $_.FullName -notlike "*\ThirdParty\*" -and
            $_.FullName -notlike "*\Engine\Render\DX12\d3dx12.h" -and
            $_.FullName -notlike "*\Apps\NocturneEditor\EditorShell.h" -and
            $_.FullName -notlike "*\Apps\NocturneEditor\EditorControls.h"
        }
).Count

$buildInfo = [ordered]@{
    schemaVersion = 1
    generator = "doxygen"
    doxygenVersion = $doxygenVersion
    documentedHeaderCount = $headerCount
    compoundCount = $compounds.Count
    html = "/api/index.html"
    xml = "/api-xml/index.xml"
    symbols = "/api-symbols.json"
    tagFile = "/api/nocturne.tag"
}

$buildInfo |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $buildInfoPath -Encoding UTF8

Write-Host "Prepared Nocturne C++ API with Doxygen $doxygenVersion."
Write-Host "Documented headers: $headerCount"
Write-Host "Indexed compounds: $($compounds.Count)"
Write-Host "HTML: Website/public/api/"
Write-Host "XML: Website/public/api-xml/"
