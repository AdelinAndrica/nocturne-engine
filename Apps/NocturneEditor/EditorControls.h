#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string_view>

namespace nocturne::editor
{
    enum class EditorButtonKind
    {
        Neutral,
        Tool,
        Primary,
        Success,
        Menu
    };

    struct EditorScrollInfo
    {
        int minimum = 0;
        int maximum = 0;
        int page = 1;
        int position = 0;
    };

    constexpr UINT WM_NOC_EDITOR_SCROLL = WM_APP + 0x240;
    constexpr UINT WM_NOC_EDITOR_SCROLL_SYNC = WM_APP + 0x241;

    bool RegisterEditorControls(HINSTANCE instance);

    HWND CreateEditorButton(HWND parent, int id, const wchar_t* text, EditorButtonKind kind);
    void SetEditorButtonActive(HWND button, bool active);

    HWND CreateEditorScrollBar(HWND parent, int id);
    void SetEditorScrollInfo(HWND scrollBar, const EditorScrollInfo& info);
    int GetEditorScrollPosition(HWND scrollBar);

    HWND CreateEditorDataTable(HWND parent, int id);
    void EditorDataTableClear(HWND table);
    void EditorDataTableAddRow(HWND table, std::wstring_view asset, std::wstring_view type);

    HWND CreateEditorTree(HWND parent, int id);
    void EditorTreeClear(HWND tree);
    void EditorTreeAddItem(HWND tree, std::wstring_view text, int depth, bool expandable, bool expanded = true);

    HWND CreateEditorInput(HWND parent, int id, const wchar_t* placeholder);
    HWND GetEditorInputEdit(HWND input);

    void AttachEditorConsoleScrollSync(HWND edit, HWND scrollBar, int lineHeight);
    void SyncEditorConsoleScroll(HWND edit, HWND scrollBar, int lineHeight);
}
