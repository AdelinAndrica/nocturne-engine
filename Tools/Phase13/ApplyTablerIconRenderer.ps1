$ErrorActionPreference = 'Stop'

$path = Join-Path $PSScriptRoot '..\..\Apps\NocturneEditor\EditorShellV3.cpp'
$path = [System.IO.Path]::GetFullPath($path)

if (-not (Test-Path $path)) {
    throw "EditorShellV3.cpp not found: $path"
}

$content = Get-Content $path -Raw

if ($content -notmatch '#include "EditorIconRenderer\.h"') {
    $content = [regex]::Replace(
        $content,
        '#include "EditorTheme\.h"\r?\n',
        "#include `"EditorTheme.h`"`r`n#include `"EditorIconRenderer.h`"`r`n",
        1)
}

if ($content -notmatch 'DrawEditorSvgIcon\(dc,\s*static_cast<EditorIconId>') {
    $pattern = 'void DrawIcon\(HDC dc, Icon icon, RECT rc, COLORREF color\)\s*\{'
    $replacement = @'
void DrawIcon(HDC dc, Icon icon, RECT rc, COLORREF color)
        {
            // Tabler SVG is the primary icon source. Keep the legacy GDI drawing below
            // only as a fallback if Direct2D SVG initialization or asset lookup fails.
            if (DrawEditorSvgIcon(dc, static_cast<EditorIconId>(static_cast<int>(icon)), rc, color))
                return;
'@

    $patched = [regex]::Replace($content, $pattern, $replacement, 1)
    if ($patched -eq $content) {
        throw 'Could not locate DrawIcon() in EditorShellV3.cpp.'
    }
    $content = $patched
}

Set-Content -Path $path -Value $content -Encoding UTF8
Write-Host "Patched: $path"
Write-Host 'Review with: git diff -- Apps/NocturneEditor/EditorShellV3.cpp'
