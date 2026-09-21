#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "EditorHierarchyModel.h"
#include "Runtime/Entity.h"

namespace nocturne::editor::shellv3
{
    // EditorShellV3-local Win32 messages. They remain internal to the editor
    // executable and are not runtime engine API.
    inline constexpr UINT WM_NOC_V3_SCROLL =
        WM_APP + 0x311;
    inline constexpr UINT WM_NOC_V3_INSPECTOR_REFRESH =
        WM_APP + 0x312;
    inline constexpr UINT WM_NOC_V3_TREE_REPARENT =
        WM_APP + 0x313;
    inline constexpr UINT WM_NOC_V3_TREE_CONTEXT =
        WM_APP + 0x314;
    inline constexpr WORD kTreeSelectionChanged =
        0x7F01;

    struct TreeReparentRequest
    {
        noc::EntityHandle child{};
        noc::EntityHandle parent{};
    };

    enum class Icon
    {
        None,
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

    enum class ButtonKind
    {
        Neutral,
        Tool,
        Primary,
        Success,
        Menu,
        IconOnly
    };

    [[nodiscard]] COLORREF Blend(
        COLORREF a,
        COLORREF b,
        int bPercent);
    void Fill(
        HDC dc,
        const RECT& rc,
        COLORREF color);
    void DrawTextUi(
        HDC dc,
        const wchar_t* text,
        RECT rc,
        COLORREF color,
        HFONT font,
        UINT flags);
    void Line(
        HDC dc,
        int x1,
        int y1,
        int x2,
        int y2,
        COLORREF color,
        int width = 1);
    void RoundBox(
        HDC dc,
        RECT rc,
        COLORREF fill,
        COLORREF border,
        int radius);

    [[nodiscard]] HFONT MakeFont(
        int px,
        int weight,
        const wchar_t* face);

    [[nodiscard]] bool RegisterV3Classes(
        HINSTANCE instance);

    [[nodiscard]] HWND MakeButton(
        HWND parent,
        int id,
        const wchar_t* text,
        Icon icon,
        ButtonKind kind,
        HFONT font);
    [[nodiscard]] HWND MakeHeader(
        HWND parent,
        const wchar_t* text,
        Icon icon,
        HFONT font);
    [[nodiscard]] HWND MakeTree(
        HWND parent,
        int id,
        HFONT font);
    [[nodiscard]] HWND MakeTable(
        HWND parent,
        int id,
        HFONT font);
    [[nodiscard]] HWND MakeScroll(
        HWND parent,
        int id);

    void ButtonActive(
        HWND hwnd,
        bool active);

    void TreeClear(HWND hwnd);
    void TreeAdd(
        HWND hwnd,
        std::wstring text,
        int depth,
        Icon icon,
        bool expandable = false,
        bool expanded = true,
        noc::EntityHandle entity =
            noc::EntityHandle::Invalid());
    void TreeCaptureExpansion(
        HWND hwnd,
        std::vector<EditorHierarchyExpansionEntry>& outEntries,
        bool& outRootExpanded);
    [[nodiscard]] noc::EntityHandle TreeSelectedEntity(
        HWND hwnd);
    void TreeSelectEntity(
        HWND hwnd,
        noc::EntityHandle entity);
    [[nodiscard]] bool TreeSelectedLabelRect(
        HWND hwnd,
        RECT& outRect);

    void TableClear(HWND hwnd);
    void TableAdd(
        HWND hwnd,
        std::wstring asset,
        std::wstring type,
        Icon icon);

    void SetScroll(
        HWND hwnd,
        int maximum,
        int page,
        int position);

    [[nodiscard]] Icon IconForType(
        const std::wstring& type);
    [[nodiscard]] std::wstring AssetTypeForPath(
        const std::filesystem::path& path,
        bool directory);

    void RichAppend(
        HWND edit,
        const std::wstring& text,
        COLORREF color);
}
