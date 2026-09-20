param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$websiteRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path

function Assert-File {
    param([string]$Relative)
    $path = Join-Path $websiteRoot $Relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing generated API file: $Relative"
    }
}

function Assert-Contains {
    param([string]$Relative, [string]$Text)
    $path = Join-Path $websiteRoot $Relative
    if (-not (Select-String -LiteralPath $path -SimpleMatch -Pattern $Text -Quiet)) {
        throw "Expected '$Text' in $Relative"
    }
}

Assert-File "public\api\index.html"
Assert-File "public\api\nocturne.tag"
Assert-File "public\api-xml\index.xml"
Assert-File "public\api-symbols.json"
Assert-File "public\api-build.json"

$aliases = @(
    "noc.Engine",
    "noc.MainLoop",
    "noc.VirtualFileSystem",
    "noc.ResourceManager",
    "noc.RenderSystem",
    "noc.RenderQueue",
    "noc.World",
    "noc.EntityHandle",
    "noc.EntityRegistry",
    "nocturne.editor.EditorShellV3",
    "nocturne.editor.EditorViewportController",
    "nocturne.editor.EditorTheme"
)

foreach ($alias in $aliases) {
    Assert-File "public\api-symbol\$alias.html"
}

$symbolIndex = Get-Content -LiteralPath (Join-Path $websiteRoot "public\api-symbols.json") -Raw | ConvertFrom-Json
$symbolNames = @($symbolIndex.compounds | ForEach-Object { [string]$_.name })

foreach ($required in @(
    "noc::Engine",
    "noc::World",
    "nocturne::editor::EditorShellV3"
)) {
    if ($required -notin $symbolNames) {
        throw "Expected API symbol '$required' in public\api-symbols.json"
    }
}

Assert-Contains "public\api-xml\index.xml" "noc::ResourceManager"
Assert-Contains "public\api-xml\index.xml" "noc::RenderSystem"

if (Select-String -LiteralPath (Join-Path $websiteRoot "public\api-xml\index.xml") -SimpleMatch -Pattern "EditorControls" -Quiet) {
    throw "Legacy EditorControls unexpectedly entered the current API index."
}

if (Select-String -LiteralPath (Join-Path $websiteRoot "public\api-xml\index.xml") -SimpleMatch -Pattern "d3dx12" -Quiet) {
    throw "Vendored d3dx12 helper unexpectedly entered the current API index."
}

Write-Host "Generated Nocturne C++ API validation passed."
