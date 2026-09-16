#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace nocturne::editor
{
    // Keep this numeric order aligned with the Phase 13 EditorShellV3::Icon enum.
    enum class EditorIconId : int
    {
        None = 0,
        Document,
        Folder,
        Save,
        Undo,
        Redo,
        Cursor,
        Move,
        Rotate,
        Scale,
        Play,
        Stop,
        Cube,
        List,
        Grid,
        Settings,
        Hierarchy,
        Viewport,
        Inspector,
        Console,
        World,
        Camera,
        Mesh,
        Texture,
        Material,
        Text,
        Metadata
    };

    // Design choice (not directly from the book): editor SVG icons are rasterized
    // through the Windows Direct2D SVG stack and cached as premultiplied BGRA bitmaps.
    // This keeps Tabler SVG files as the source of truth while allowing existing GDI
    // editor controls to remain in Phase 13.
    bool DrawEditorSvgIcon(HDC dc, EditorIconId icon, const RECT& rect, COLORREF color);
    void ShutdownEditorIconRenderer();
}
