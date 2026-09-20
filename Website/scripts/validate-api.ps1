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

function Get-DescriptionText {
    param([System.Xml.XmlNode]$Node)

    if ($null -eq $Node) {
        return ""
    }

    $briefNode = $Node.SelectSingleNode("briefdescription")
    $detailNode = $Node.SelectSingleNode("detaileddescription")

    $brief = if ($null -ne $briefNode) { [string]$briefNode.InnerText } else { "" }
    $detail = if ($null -ne $detailNode) { [string]$detailNode.InnerText } else { "" }

    return ($brief + " " + $detail).Trim()
}

function Assert-BeginnerDocumentedCompound {
    param(
        [string]$QualifiedName,
        [switch]$RequireAllPublicFunctions
    )

    $entry = $symbolIndex.compounds |
        Where-Object { [string]$_.name -eq $QualifiedName } |
        Select-Object -First 1

    if (-not $entry) {
        throw "Missing API symbol '$QualifiedName' in symbol index."
    }

    $xmlPath = Join-Path $websiteRoot ("public\api-xml\" + [string]$entry.refid + ".xml")
    if (-not (Test-Path -LiteralPath $xmlPath -PathType Leaf)) {
        throw "Missing compound XML for '$QualifiedName': $xmlPath"
    }

    [xml]$compoundXml = Get-Content -LiteralPath $xmlPath -Raw
    $compound = $compoundXml.doxygen.compounddef

    $description = Get-DescriptionText $compound
    if ([string]::IsNullOrWhiteSpace($description)) {
        throw "Beginner API contract: '$QualifiedName' has no generated class/struct description."
    }

    if ($RequireAllPublicFunctions) {
        $functions = @(
            $compound.sectiondef.memberdef |
            Where-Object {
                [string]$_.kind -eq "function" -and
                [string]$_.prot -eq "public"
            }
        )

        foreach ($fn in $functions) {
            $name = [string]$fn.name
            $fnDescription = Get-DescriptionText $fn
            if ([string]::IsNullOrWhiteSpace($fnDescription)) {
                throw "Beginner API contract: '$QualifiedName::$name' has no generated public-function description."
            }
        }
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

# Web 7.1 beginner-oriented contract: these public surfaces must never regress
# back to signature-only documentation.
foreach ($compound in @(
    "noc::Engine",
    "noc::MainLoop",
    "noc::VirtualFileSystem",
    "noc::ResourceManager",
    "noc::ResourceHandle",
    "noc::EntityHandle",
    "noc::EntityRegistry",
    "noc::World",
    "noc::RenderQueue",
    "noc::RenderSystem",
    "noc::Dx12Renderer",
    "nocturne::editor::EditorShellV3",
    "nocturne::editor::EditorViewportController",
    "nocturne::editor::EditorTheme"
)) {
    Assert-BeginnerDocumentedCompound -QualifiedName $compound -RequireAllPublicFunctions
}

foreach ($component in @(
    "noc::TransformComponent",
    "noc::RenderableComponent",
    "noc::CameraComponent",
    "noc::NameComponent"
)) {
    Assert-BeginnerDocumentedCompound -QualifiedName $component
}

Assert-Contains "public\api\index.html" "Beginner Guide"

if (Select-String -LiteralPath (Join-Path $websiteRoot "public\api-xml\index.xml") -SimpleMatch -Pattern "EditorControls" -Quiet) {
    throw "Legacy EditorControls unexpectedly entered the current API index."
}

if (Select-String -LiteralPath (Join-Path $websiteRoot "public\api-xml\index.xml") -SimpleMatch -Pattern "d3dx12" -Quiet) {
    throw "Vendored d3dx12 helper unexpectedly entered the current API index."
}

Write-Host "Generated Nocturne C++ API validation passed, including beginner documentation coverage."
