#include "EditorControls.h"
#include "EditorTheme.h"

#include <algorithm>
#include <string>
#include <vector>

#include <CommCtrl.h>

namespace nocturne::editor
{
    namespace
    {
        constexpr wchar_t kButtonClass[] = L"NocturneEditorButton";
        constexpr wchar_t kScrollClass[] = L"NocturneEditorScrollBar";
        constexpr wchar_t kTableClass[] = L"NocturneEditorDataTable";
        constexpr wchar_t kTreeClass[] = L"NocturneEditorTree";
        constexpr wchar_t kInputClass[] = L"NocturneEditorInput";
        constexpr UINT WM_NOC_BUTTON_ACTIVE = WM_APP + 0x250;

        COLORREF Blend(COLORREF a, COLORREF b, int bPercent)
        {
            const int aPercent = 100 - bPercent;
            return RGB(
                (GetRValue(a) * aPercent + GetRValue(b) * bPercent) / 100,
                (GetGValue(a) * aPercent + GetGValue(b) * bPercent) / 100,
                (GetBValue(a) * aPercent + GetBValue(b) * bPercent) / 100);
        }

        void Fill(HDC dc, const RECT& rc, COLORREF color)
        {
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rc, brush);
            DeleteObject(brush);
        }

        void RoundBox(HDC dc, RECT rc, COLORREF fill, COLORREF border, int radius)
        {
            HBRUSH brush = CreateSolidBrush(fill);
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldBrush = SelectObject(dc, brush);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }

        void DrawTextSimple(HDC dc, const wchar_t* text, RECT rc, COLORREF color, HFONT font, UINT flags)
        {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, color);
            HGDIOBJ oldFont = nullptr;
            if (font) oldFont = SelectObject(dc, font);
            DrawTextW(dc, text, -1, &rc, flags);
            if (oldFont) SelectObject(dc, oldFont);
        }

        struct ButtonCreateInfo
        {
            EditorButtonKind kind = EditorButtonKind::Neutral;
        };

        struct ButtonState
        {
            EditorButtonKind kind = EditorButtonKind::Neutral;
            HFONT font = nullptr;
            bool hovered = false;
            bool pressed = false;
            bool active = false;
        };

        LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<ButtonState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                auto* init = reinterpret_cast<ButtonCreateInfo*>(cs->lpCreateParams);
                auto* created = new ButtonState{};
                if (init) created->kind = init->kind;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_NCDESTROY:
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                return 0;
            case WM_SETFONT:
                if (state) state->font = reinterpret_cast<HFONT>(wParam);
                if (lParam) InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            case WM_NOC_BUTTON_ACTIVE:
                if (state)
                {
                    state->active = wParam != 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (state && !state->hovered)
                {
                    state->hovered = true;
                    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                    TrackMouseEvent(&tme);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSELEAVE:
                if (state)
                {
                    state->hovered = false;
                    state->pressed = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    state->pressed = true;
                    SetCapture(hwnd);
                    SetFocus(hwnd);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_LBUTTONUP:
                if (state)
                {
                    const bool wasPressed = state->pressed;
                    state->pressed = false;
                    ReleaseCapture();
                    RECT rc{};
                    GetClientRect(hwnd, &rc);
                    POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    if (wasPressed && PtInRect(&rc, p))
                    {
                        SendMessageW(GetParent(hwnd), WM_COMMAND,
                            MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_KEYDOWN:
                if (wParam == VK_SPACE || wParam == VK_RETURN)
                {
                    SendMessageW(GetParent(hwnd), WM_COMMAND,
                        MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
                    return 0;
                }
                break;
            case WM_SETFOCUS:
            case WM_KILLFOCUS:
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const auto& c = EditorTheme::Colors();
                const auto& m = EditorTheme::Metrics();

                COLORREF bg = c.buttonBg;
                COLORREF border = c.border;
                COLORREF text = c.textPrimary;

                if (state)
                {
                    if (state->kind == EditorButtonKind::Menu)
                    {
                        bg = state->hovered || state->pressed ? c.panelBgAlt : c.toolbarBg;
                        border = bg;
                    }
                    else if (state->kind == EditorButtonKind::Success)
                    {
                        bg = state->pressed ? Blend(c.buttonBg, c.windowBg, 24)
                            : state->hovered ? Blend(c.buttonBg, c.success, 10) : c.buttonBg;
                        border = state->hovered ? Blend(c.border, c.success, 28) : c.border;
                        text = c.success;
                    }
                    else if (state->kind == EditorButtonKind::Primary || state->active)
                    {
                        bg = state->pressed ? Blend(c.accent, c.windowBg, 24)
                            : state->hovered ? c.accentHover : c.accent;
                        border = bg;
                        text = RGB(238, 246, 255);
                    }
                    else
                    {
                        bg = state->pressed ? Blend(c.buttonBg, c.windowBg, 24)
                            : state->hovered ? c.buttonHover : c.buttonBg;
                        border = state->hovered ? Blend(c.border, c.textMuted, 18) : c.border;
                    }
                }

                Fill(dc, rc, c.toolbarBg);
                RECT box = rc;
                InflateRect(&box, -1, -1);
                RoundBox(dc, box, bg, border, m.buttonRadius);

                if (GetFocus() == hwnd && state && state->kind != EditorButtonKind::Menu)
                {
                    RECT focus = box;
                    InflateRect(&focus, -2, -2);
                    HPEN pen = CreatePen(PS_SOLID, 1, Blend(c.accent, c.border, 55));
                    HGDIOBJ oldPen = SelectObject(dc, pen);
                    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                    RoundRect(dc, focus.left, focus.top, focus.right, focus.bottom,
                        (std::max)(2, m.buttonRadius - 2), (std::max)(2, m.buttonRadius - 2));
                    SelectObject(dc, oldBrush);
                    SelectObject(dc, oldPen);
                    DeleteObject(pen);
                }

                wchar_t label[128]{};
                GetWindowTextW(hwnd, label, static_cast<int>(std::size(label)));
                DrawTextSimple(dc, label, rc, text, state ? state->font : nullptr,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                EndPaint(hwnd, &ps);
                return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        struct ScrollState
        {
            EditorScrollInfo info{};
            bool hovered = false;
            bool thumbHovered = false;
            bool dragging = false;
            int dragOffset = 0;
        };

        int ScrollMaxPos(const ScrollState& state)
        {
            return (std::max)(state.info.minimum,
                state.info.maximum - (std::max)(1, state.info.page));
        }

        RECT ScrollThumbRect(HWND hwnd, const ScrollState& state)
        {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int height = (std::max)(1, rc.bottom - rc.top);
            const int total = (std::max)(1, state.info.maximum - state.info.minimum);
            const int page = (std::max)(1, state.info.page);
            int thumbHeight = (height * page) / (std::max)(page, total);
            thumbHeight = (std::clamp)(thumbHeight, 24, height);

            const int maxPos = ScrollMaxPos(state);
            const int travel = (std::max)(0, height - thumbHeight);
            int top = 0;
            if (maxPos > state.info.minimum && travel > 0)
            {
                const double t = static_cast<double>(state.info.position - state.info.minimum) /
                    static_cast<double>(maxPos - state.info.minimum);
                top = static_cast<int>(t * travel + 0.5);
            }
            return RECT{ 1, top + 1, (std::max)(2, rc.right - 1),
                (std::min)(rc.bottom - 1, top + thumbHeight - 1) };
        }

        void NotifyScroll(HWND hwnd, ScrollState& state, int position)
        {
            const int maxPos = ScrollMaxPos(state);
            state.info.position = (std::clamp)(position, state.info.minimum, maxPos);
            SendMessageW(GetParent(hwnd), WM_NOC_EDITOR_SCROLL,
                static_cast<WPARAM>(GetDlgCtrlID(hwnd)), static_cast<LPARAM>(state.info.position));
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        LRESULT CALLBACK ScrollProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<ScrollState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* created = new ScrollState{};
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_NCDESTROY:
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                return 0;
            case WM_MOUSEMOVE:
                if (state)
                {
                    if (!state->hovered)
                    {
                        state->hovered = true;
                        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                        TrackMouseEvent(&tme);
                    }
                    POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    const RECT thumb = ScrollThumbRect(hwnd, *state);
                    const bool overThumb = PtInRect(&thumb, p) != FALSE;
                    if (overThumb != state->thumbHovered)
                    {
                        state->thumbHovered = overThumb;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    if (state->dragging)
                    {
                        RECT rc{};
                        GetClientRect(hwnd, &rc);
                        const int thumbHeight = thumb.bottom - thumb.top;
                        const int travel = (std::max)(1, rc.bottom - thumbHeight);
                        const int desired = (std::clamp)(p.y - state->dragOffset, 0, travel);
                        const double t = static_cast<double>(desired) / static_cast<double>(travel);
                        const int maxPos = ScrollMaxPos(*state);
                        NotifyScroll(hwnd, *state, state->info.minimum +
                            static_cast<int>(t * (maxPos - state->info.minimum) + 0.5));
                    }
                }
                return 0;
            case WM_MOUSELEAVE:
                if (state)
                {
                    state->hovered = false;
                    state->thumbHovered = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    const RECT thumb = ScrollThumbRect(hwnd, *state);
                    if (PtInRect(&thumb, p))
                    {
                        state->dragging = true;
                        state->dragOffset = p.y - thumb.top;
                        SetCapture(hwnd);
                    }
                    else
                    {
                        const int delta = p.y < thumb.top ? -state->info.page : state->info.page;
                        NotifyScroll(hwnd, *state, state->info.position + delta);
                    }
                }
                return 0;
            case WM_LBUTTONUP:
                if (state && state->dragging)
                {
                    state->dragging = false;
                    ReleaseCapture();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const auto& c = EditorTheme::Colors();
                Fill(dc, rc, c.inputBg);
                if (state && ScrollMaxPos(*state) > state->info.minimum)
                {
                    RECT thumb = ScrollThumbRect(hwnd, *state);
                    const COLORREF color = state->dragging ? Blend(c.textMuted, c.accent, 42)
                        : state->thumbHovered ? Blend(c.border, c.textMuted, 45)
                        : Blend(c.border, c.textMuted, 25);
                    HBRUSH brush = CreateSolidBrush(color);
                    HGDIOBJ oldBrush = SelectObject(dc, brush);
                    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                    RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 8, 8);
                    SelectObject(dc, oldPen);
                    SelectObject(dc, oldBrush);
                    DeleteObject(brush);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        struct TableRow
        {
            std::wstring asset;
            std::wstring type;
        };

        struct TableState
        {
            std::vector<TableRow> rows;
            HWND scroll = nullptr;
            HFONT font = nullptr;
            int firstRow = 0;
            int selected = -1;
            int hover = -1;
        };

        void SyncTableScroll(HWND hwnd, TableState& state)
        {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const auto& m = EditorTheme::Metrics();
            const int visible = (std::max)(1, (rc.bottom - m.tableHeaderHeight) / m.tableRowHeight);
            EditorScrollInfo info{};
            info.maximum = static_cast<int>(state.rows.size());
            info.page = visible;
            info.position = state.firstRow;
            SetEditorScrollInfo(state.scroll, info);
            state.firstRow = GetEditorScrollPosition(state.scroll);
        }

        LRESULT CALLBACK TableProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<TableState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* created = new TableState{};
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_CREATE:
                if (state) state->scroll = CreateEditorScrollBar(hwnd, 1);
                return 0;
            case WM_NCDESTROY:
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                return 0;
            case WM_SETFONT:
                if (state) state->font = reinterpret_cast<HFONT>(wParam);
                if (lParam) InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            case WM_SIZE:
                if (state && state->scroll)
                {
                    const auto& m = EditorTheme::Metrics();
                    MoveWindow(state->scroll, (std::max)(0, LOWORD(lParam) - m.scrollbarWidth - 2),
                        m.tableHeaderHeight + 2, m.scrollbarWidth,
                        (std::max)(0, HIWORD(lParam) - m.tableHeaderHeight - 4), TRUE);
                    SyncTableScroll(hwnd, *state);
                }
                return 0;
            case WM_NOC_EDITOR_SCROLL:
                if (state && static_cast<int>(wParam) == 1)
                {
                    state->firstRow = static_cast<int>(lParam);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEWHEEL:
                if (state)
                {
                    const int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                    state->firstRow = (std::max)(0, state->firstRow - steps * 3);
                    SyncTableScroll(hwnd, *state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (state)
                {
                    const auto& m = EditorTheme::Metrics();
                    int hover = -1;
                    const int y = GET_Y_LPARAM(lParam);
                    if (y >= m.tableHeaderHeight)
                    {
                        const int row = state->firstRow + (y - m.tableHeaderHeight) / m.tableRowHeight;
                        if (row >= 0 && row < static_cast<int>(state->rows.size())) hover = row;
                    }
                    if (hover != state->hover)
                    {
                        state->hover = hover;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                    TrackMouseEvent(&tme);
                }
                return 0;
            case WM_MOUSELEAVE:
                if (state)
                {
                    state->hover = -1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    const auto& m = EditorTheme::Metrics();
                    const int y = GET_Y_LPARAM(lParam);
                    if (y >= m.tableHeaderHeight)
                    {
                        const int row = state->firstRow + (y - m.tableHeaderHeight) / m.tableRowHeight;
                        if (row >= 0 && row < static_cast<int>(state->rows.size()))
                        {
                            state->selected = row;
                            SetFocus(hwnd);
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                }
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const auto& c = EditorTheme::Colors();
                const auto& m = EditorTheme::Metrics();
                Fill(dc, rc, c.inputBg);

                RECT header{ 0, 0, rc.right, m.tableHeaderHeight };
                Fill(dc, header, c.panelBgAlt);
                HPEN separator = CreatePen(PS_SOLID, 1, c.border);
                HGDIOBJ oldPen = SelectObject(dc, separator);
                MoveToEx(dc, 0, m.tableHeaderHeight - 1, nullptr);
                LineTo(dc, rc.right, m.tableHeaderHeight - 1);
                SelectObject(dc, oldPen);
                DeleteObject(separator);

                const int usableW = (std::max)(120, rc.right - m.scrollbarWidth - 6);
                const int typeW = (std::clamp)(usableW / 3, 90, 150);
                const int assetW = usableW - typeW;
                RECT assetHeader{ 12, 0, assetW - 6, m.tableHeaderHeight };
                RECT typeHeader{ assetW + 8, 0, usableW - 8, m.tableHeaderHeight };
                DrawTextSimple(dc, L"Asset", assetHeader, c.textMuted, state ? state->font : nullptr,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextSimple(dc, L"Type", typeHeader, c.textMuted, state ? state->font : nullptr,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                if (state && state->rows.empty())
                {
                    RECT empty = rc;
                    empty.top = m.tableHeaderHeight;
                    DrawTextSimple(dc, L"No assets in this folder", empty, c.textMuted, state->font,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                else if (state)
                {
                    int y = m.tableHeaderHeight;
                    for (int row = state->firstRow;
                        row < static_cast<int>(state->rows.size()) && y < rc.bottom;
                        ++row, y += m.tableRowHeight)
                    {
                        RECT rowRc{ 0, y, usableW, (std::min)(rc.bottom, y + m.tableRowHeight) };
                        if (row == state->selected)
                            Fill(dc, rowRc, Blend(c.panelBg, c.accent, 32));
                        else if (row == state->hover)
                            Fill(dc, rowRc, Blend(c.inputBg, c.panelBgAlt, 58));

                        RECT assetRc{ 12, y, assetW - 6, y + m.tableRowHeight };
                        RECT typeRc{ assetW + 8, y, usableW - 8, y + m.tableRowHeight };
                        DrawTextSimple(dc, state->rows[row].asset.c_str(), assetRc, c.textPrimary, state->font,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                        DrawTextSimple(dc, state->rows[row].type.c_str(), typeRc, c.textMuted, state->font,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        struct TreeItem
        {
            std::wstring text;
            int depth = 0;
            bool expandable = false;
            bool expanded = true;
        };

        struct TreeState
        {
            std::vector<TreeItem> items;
            HWND scroll = nullptr;
            HFONT font = nullptr;
            int firstRow = 0;
            int selected = -1;
            int hover = -1;
        };

        std::vector<int> VisibleTreeIndices(const TreeState& state)
        {
            std::vector<int> result;
            std::vector<bool> expandedAtDepth(16, true);
            for (int i = 0; i < static_cast<int>(state.items.size()); ++i)
            {
                const auto& item = state.items[i];
                bool visible = true;
                for (int d = 0; d < item.depth && d < static_cast<int>(expandedAtDepth.size()); ++d)
                {
                    if (!expandedAtDepth[d])
                    {
                        visible = false;
                        break;
                    }
                }
                if (visible) result.push_back(i);
                if (item.depth < static_cast<int>(expandedAtDepth.size()))
                {
                    expandedAtDepth[item.depth] = !item.expandable || item.expanded;
                    for (int d = item.depth + 1; d < static_cast<int>(expandedAtDepth.size()); ++d)
                        expandedAtDepth[d] = true;
                }
            }
            return result;
        }

        void SyncTreeScroll(HWND hwnd, TreeState& state)
        {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const auto& m = EditorTheme::Metrics();
            const int visibleRows = (std::max)(1, rc.bottom / m.treeRowHeight);
            const auto visible = VisibleTreeIndices(state);
            EditorScrollInfo info{};
            info.maximum = static_cast<int>(visible.size());
            info.page = visibleRows;
            info.position = state.firstRow;
            SetEditorScrollInfo(state.scroll, info);
            state.firstRow = GetEditorScrollPosition(state.scroll);
        }

        LRESULT CALLBACK TreeProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<TreeState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* created = new TreeState{};
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_CREATE:
                if (state) state->scroll = CreateEditorScrollBar(hwnd, 1);
                return 0;
            case WM_NCDESTROY:
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                return 0;
            case WM_SETFONT:
                if (state) state->font = reinterpret_cast<HFONT>(wParam);
                if (lParam) InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            case WM_SIZE:
                if (state && state->scroll)
                {
                    const auto& m = EditorTheme::Metrics();
                    MoveWindow(state->scroll, (std::max)(0, LOWORD(lParam) - m.scrollbarWidth - 2), 2,
                        m.scrollbarWidth, (std::max)(0, HIWORD(lParam) - 4), TRUE);
                    SyncTreeScroll(hwnd, *state);
                }
                return 0;
            case WM_NOC_EDITOR_SCROLL:
                if (state && static_cast<int>(wParam) == 1)
                {
                    state->firstRow = static_cast<int>(lParam);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEWHEEL:
                if (state)
                {
                    const int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                    state->firstRow = (std::max)(0, state->firstRow - steps * 3);
                    SyncTreeScroll(hwnd, *state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (state)
                {
                    const auto visible = VisibleTreeIndices(*state);
                    const int localRow = GET_Y_LPARAM(lParam) / EditorTheme::Metrics().treeRowHeight;
                    const int visibleIndex = state->firstRow + localRow;
                    const int hover = visibleIndex >= 0 && visibleIndex < static_cast<int>(visible.size())
                        ? visible[visibleIndex] : -1;
                    if (hover != state->hover)
                    {
                        state->hover = hover;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                    TrackMouseEvent(&tme);
                }
                return 0;
            case WM_MOUSELEAVE:
                if (state)
                {
                    state->hover = -1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    const auto& m = EditorTheme::Metrics();
                    const auto visible = VisibleTreeIndices(*state);
                    const int visibleIndex = state->firstRow + GET_Y_LPARAM(lParam) / m.treeRowHeight;
                    if (visibleIndex >= 0 && visibleIndex < static_cast<int>(visible.size()))
                    {
                        const int index = visible[visibleIndex];
                        state->selected = index;
                        const int arrowLeft = 10 + state->items[index].depth * 18;
                        const int x = GET_X_LPARAM(lParam);
                        if (state->items[index].expandable && x >= arrowLeft && x <= arrowLeft + 18)
                            state->items[index].expanded = !state->items[index].expanded;
                        SyncTreeScroll(hwnd, *state);
                        SetFocus(hwnd);
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const auto& c = EditorTheme::Colors();
                const auto& m = EditorTheme::Metrics();
                Fill(dc, rc, c.panelBg);

                if (state)
                {
                    const auto visible = VisibleTreeIndices(*state);
                    int y = 0;
                    for (int v = state->firstRow;
                        v < static_cast<int>(visible.size()) && y < rc.bottom;
                        ++v, y += m.treeRowHeight)
                    {
                        const int index = visible[v];
                        const auto& item = state->items[index];
                        RECT row{ 0, y, (std::max)(0, rc.right - m.scrollbarWidth - 4), y + m.treeRowHeight };
                        if (index == state->selected)
                            Fill(dc, row, Blend(c.panelBg, c.accent, 40));
                        else if (index == state->hover)
                            Fill(dc, row, c.panelBgAlt);

                        const int arrowX = 10 + item.depth * 18;
                        if (item.expandable)
                        {
                            POINT pts[3]{};
                            if (item.expanded)
                            {
                                pts[0] = { arrowX + 3, y + 9 };
                                pts[1] = { arrowX + 11, y + 9 };
                                pts[2] = { arrowX + 7, y + 14 };
                            }
                            else
                            {
                                pts[0] = { arrowX + 5, y + 7 };
                                pts[1] = { arrowX + 5, y + 15 };
                                pts[2] = { arrowX + 10, y + 11 };
                            }
                            HBRUSH brush = CreateSolidBrush(c.textMuted);
                            HGDIOBJ oldBrush = SelectObject(dc, brush);
                            HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                            Polygon(dc, pts, 3);
                            SelectObject(dc, oldPen);
                            SelectObject(dc, oldBrush);
                            DeleteObject(brush);
                        }

                        RECT textRc{ arrowX + 20, y, row.right - 6, y + m.treeRowHeight };
                        DrawTextSimple(dc, item.text.c_str(), textRc, c.textPrimary, state->font,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        struct InputCreateInfo
        {
            const wchar_t* placeholder = L"";
        };

        struct InputState
        {
            HWND edit = nullptr;
            HFONT font = nullptr;
            HBRUSH brush = nullptr;
            std::wstring placeholder;
            bool focused = false;
        };

        LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<InputState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                auto* init = reinterpret_cast<InputCreateInfo*>(cs->lpCreateParams);
                auto* created = new InputState{};
                if (init && init->placeholder) created->placeholder = init->placeholder;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_CREATE:
                if (state)
                {
                    state->brush = CreateSolidBrush(EditorTheme::Colors().inputBg);
                    state->edit = CreateWindowExW(0, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(1),
                        GetModuleHandleW(nullptr), nullptr);
                    SendMessageW(state->edit, EM_SETCUEBANNER, TRUE,
                        reinterpret_cast<LPARAM>(state->placeholder.c_str()));
                }
                return 0;
            case WM_NCDESTROY:
                if (state)
                {
                    if (state->brush) DeleteObject(state->brush);
                    delete state;
                }
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                return 0;
            case WM_SETFONT:
                if (state)
                {
                    state->font = reinterpret_cast<HFONT>(wParam);
                    if (state->edit) SendMessageW(state->edit, WM_SETFONT, wParam, lParam);
                }
                return 0;
            case WM_SIZE:
                if (state && state->edit)
                    MoveWindow(state->edit, 10, 4, (std::max)(0, LOWORD(lParam) - 20),
                        (std::max)(0, HIWORD(lParam) - 8), TRUE);
                return 0;
            case WM_COMMAND:
                if (state && reinterpret_cast<HWND>(lParam) == state->edit)
                {
                    const int code = HIWORD(wParam);
                    if (code == EN_SETFOCUS) state->focused = true;
                    if (code == EN_KILLFOCUS) state->focused = false;
                    if (code == EN_SETFOCUS || code == EN_KILLFOCUS)
                        InvalidateRect(hwnd, nullptr, FALSE);
                    SendMessageW(GetParent(hwnd), WM_COMMAND,
                        MAKEWPARAM(GetDlgCtrlID(hwnd), code), reinterpret_cast<LPARAM>(hwnd));
                    return 0;
                }
                break;
            case WM_CTLCOLOREDIT:
                if (state)
                {
                    HDC dc = reinterpret_cast<HDC>(wParam);
                    SetTextColor(dc, EditorTheme::Colors().textPrimary);
                    SetBkColor(dc, EditorTheme::Colors().inputBg);
                    return reinterpret_cast<LRESULT>(state->brush);
                }
                break;
            case WM_LBUTTONDOWN:
                if (state && state->edit) SetFocus(state->edit);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const auto& c = EditorTheme::Colors();
                RoundBox(dc, rc, c.inputBg, state && state->focused ? c.accent : c.border,
                    EditorTheme::Metrics().inputRadius);
                EndPaint(hwnd, &ps);
                return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        struct ConsoleSyncState
        {
            HWND scroll = nullptr;
            int lineHeight = 18;
        };

        LRESULT CALLBACK ConsoleSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
            UINT_PTR, DWORD_PTR refData)
        {
            auto* state = reinterpret_cast<ConsoleSyncState*>(refData);
            const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (state && (msg == WM_MOUSEWHEEL || msg == WM_KEYDOWN ||
                msg == WM_LBUTTONUP || msg == WM_SIZE))
            {
                SyncEditorConsoleScroll(hwnd, state->scroll, state->lineHeight);
            }
            if (msg == WM_NCDESTROY)
            {
                RemoveWindowSubclass(hwnd, ConsoleSubclassProc, 1);
                delete state;
            }
            return result;
        }

        bool RegisterClass(HINSTANCE instance, const wchar_t* name, WNDPROC proc, HCURSOR cursor)
        {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.hInstance = instance;
            wc.lpfnWndProc = proc;
            wc.lpszClassName = name;
            wc.hCursor = cursor;
            wc.hbrBackground = nullptr;
            wc.style = CS_HREDRAW | CS_VREDRAW;
            if (RegisterClassExW(&wc) != 0) return true;
            return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
    }

    bool RegisterEditorControls(HINSTANCE instance)
    {
        return RegisterClass(instance, kButtonClass, ButtonProc, LoadCursorW(nullptr, IDC_HAND))
            && RegisterClass(instance, kScrollClass, ScrollProc, LoadCursorW(nullptr, IDC_ARROW))
            && RegisterClass(instance, kTableClass, TableProc, LoadCursorW(nullptr, IDC_ARROW))
            && RegisterClass(instance, kTreeClass, TreeProc, LoadCursorW(nullptr, IDC_ARROW))
            && RegisterClass(instance, kInputClass, InputProc, LoadCursorW(nullptr, IDC_IBEAM));
    }

    HWND CreateEditorButton(HWND parent, int id, const wchar_t* text, EditorButtonKind kind)
    {
        ButtonCreateInfo init{ kind };
        return CreateWindowExW(0, kButtonClass, text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), &init);
    }

    void SetEditorButtonActive(HWND button, bool active)
    {
        if (button) SendMessageW(button, WM_NOC_BUTTON_ACTIVE, active ? TRUE : FALSE, 0);
    }

    HWND CreateEditorScrollBar(HWND parent, int id)
    {
        return CreateWindowExW(0, kScrollClass, L"", WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
    }

    void SetEditorScrollInfo(HWND scrollBar, const EditorScrollInfo& info)
    {
        if (!scrollBar) return;
        auto* state = reinterpret_cast<ScrollState*>(GetWindowLongPtrW(scrollBar, GWLP_USERDATA));
        if (!state) return;
        state->info = info;
        state->info.position = (std::clamp)(state->info.position,
            state->info.minimum, ScrollMaxPos(*state));
        InvalidateRect(scrollBar, nullptr, FALSE);
    }

    int GetEditorScrollPosition(HWND scrollBar)
    {
        if (!scrollBar) return 0;
        auto* state = reinterpret_cast<ScrollState*>(GetWindowLongPtrW(scrollBar, GWLP_USERDATA));
        return state ? state->info.position : 0;
    }

    HWND CreateEditorDataTable(HWND parent, int id)
    {
        return CreateWindowExW(0, kTableClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
    }

    void EditorDataTableClear(HWND table)
    {
        auto* state = table ? reinterpret_cast<TableState*>(GetWindowLongPtrW(table, GWLP_USERDATA)) : nullptr;
        if (!state) return;
        state->rows.clear();
        state->firstRow = 0;
        state->selected = -1;
        state->hover = -1;
        SyncTableScroll(table, *state);
        InvalidateRect(table, nullptr, FALSE);
    }

    void EditorDataTableAddRow(HWND table, std::wstring_view asset, std::wstring_view type)
    {
        auto* state = table ? reinterpret_cast<TableState*>(GetWindowLongPtrW(table, GWLP_USERDATA)) : nullptr;
        if (!state) return;
        state->rows.push_back({ std::wstring(asset), std::wstring(type) });
        SyncTableScroll(table, *state);
        InvalidateRect(table, nullptr, FALSE);
    }

    HWND CreateEditorTree(HWND parent, int id)
    {
        return CreateWindowExW(0, kTreeClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
    }

    void EditorTreeClear(HWND tree)
    {
        auto* state = tree ? reinterpret_cast<TreeState*>(GetWindowLongPtrW(tree, GWLP_USERDATA)) : nullptr;
        if (!state) return;
        state->items.clear();
        state->firstRow = 0;
        state->selected = -1;
        state->hover = -1;
        SyncTreeScroll(tree, *state);
        InvalidateRect(tree, nullptr, FALSE);
    }

    void EditorTreeAddItem(HWND tree, std::wstring_view text, int depth, bool expandable, bool expanded)
    {
        auto* state = tree ? reinterpret_cast<TreeState*>(GetWindowLongPtrW(tree, GWLP_USERDATA)) : nullptr;
        if (!state) return;
        state->items.push_back({ std::wstring(text), depth, expandable, expanded });
        if (state->selected < 0) state->selected = 0;
        SyncTreeScroll(tree, *state);
        InvalidateRect(tree, nullptr, FALSE);
    }

    HWND CreateEditorInput(HWND parent, int id, const wchar_t* placeholder)
    {
        InputCreateInfo init{ placeholder };
        return CreateWindowExW(0, kInputClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), &init);
    }

    HWND GetEditorInputEdit(HWND input)
    {
        auto* state = input ? reinterpret_cast<InputState*>(GetWindowLongPtrW(input, GWLP_USERDATA)) : nullptr;
        return state ? state->edit : nullptr;
    }

    void AttachEditorConsoleScrollSync(HWND edit, HWND scrollBar, int lineHeight)
    {
        if (!edit || !scrollBar) return;
        auto* state = new ConsoleSyncState{ scrollBar, lineHeight };
        SetWindowSubclass(edit, ConsoleSubclassProc, 1, reinterpret_cast<DWORD_PTR>(state));
        SyncEditorConsoleScroll(edit, scrollBar, lineHeight);
    }

    void SyncEditorConsoleScroll(HWND edit, HWND scrollBar, int lineHeight)
    {
        if (!edit || !scrollBar) return;
        RECT rc{};
        GetClientRect(edit, &rc);
        const int lineCount = static_cast<int>(SendMessageW(edit, EM_GETLINECOUNT, 0, 0));
        const int firstLine = static_cast<int>(SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
        const int visible = (std::max)(1, (rc.bottom - rc.top) / (std::max)(1, lineHeight));
        EditorScrollInfo info{};
        info.maximum = lineCount;
        info.page = visible;
        info.position = firstLine;
        SetEditorScrollInfo(scrollBar, info);
    }
}
