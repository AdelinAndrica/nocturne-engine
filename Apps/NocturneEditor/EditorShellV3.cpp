#include "EditorShellV3.h"
#include "EditorTheme.h"
#include "EditorIconRenderer.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <CommCtrl.h>
#include <Windowsx.h>
#include <dwmapi.h>
#include <Richedit.h>

#include "Core/Log.h"
#include "Runtime/Engine.h"
#include "Runtime/World.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace nocturne::editor
{
    namespace
    {
        constexpr wchar_t kButtonClass[] = L"NocturneV3Button";
        constexpr wchar_t kHeaderClass[] = L"NocturneV3Header";
        constexpr wchar_t kTreeClass[] = L"NocturneV3Tree";
        constexpr wchar_t kTableClass[] = L"NocturneV3Table";
        constexpr wchar_t kScrollClass[] = L"NocturneV3Scroll";
        constexpr UINT WM_NOC_V3_ACTIVE = WM_APP + 0x310;
        constexpr UINT WM_NOC_V3_SCROLL = WM_APP + 0x311;

        enum class Icon
        {
            None, Document, Folder, Save, Undo, Redo, Cursor, Move, Rotate, Scale,
            Play, Stop, Cube, List, Grid, Settings, Hierarchy, Viewport, Inspector,
            Console, World, Camera, Mesh, Texture, Material, Text, Metadata
        };

        enum class ButtonKind { Neutral, Tool, Primary, Success, Menu, IconOnly };

        struct ButtonInit { Icon icon = Icon::None; ButtonKind kind = ButtonKind::Neutral; };
        struct ButtonState
        {
            Icon icon = Icon::None;
            ButtonKind kind = ButtonKind::Neutral;
            std::wstring label;
            HFONT font = nullptr;
            bool hover = false;
            bool pressed = false;
            bool active = false;
        };
        struct HeaderInit { Icon icon = Icon::None; };
        struct HeaderState { Icon icon = Icon::None; std::wstring title; HFONT font = nullptr; };
        struct TreeItem { std::wstring text; int depth = 0; Icon icon = Icon::None; bool expandable = false; bool expanded = true; };
        struct TreeState { std::vector<TreeItem> items; HFONT font = nullptr; int firstRow = 0; int selected = -1; int hover = -1; };
        struct TableRow { std::wstring asset; std::wstring type; Icon icon = Icon::None; };
        struct TableState { std::vector<TableRow> rows; HFONT font = nullptr; int firstRow = 0; int selected = -1; int hover = -1; };
        struct ScrollState { int minimum = 0; int maximum = 0; int page = 1; int position = 0; bool thumbHover = false; bool dragging = false; int dragOffset = 0; };

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

        void DrawTextUi(HDC dc, const wchar_t* text, RECT rc, COLORREF color, HFONT font, UINT flags)
        {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, color);
            HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
            DrawTextW(dc, text, -1, &rc, flags);
            if (oldFont) SelectObject(dc, oldFont);
        }

        void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1)
        {
            HPEN pen = CreatePen(PS_SOLID, width, color);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            MoveToEx(dc, x1, y1, nullptr);
            LineTo(dc, x2, y2);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
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

        HFONT MakeFont(int px, int weight, const wchar_t* face)
        {
            return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
        }

        void DrawIcon(HDC dc, Icon icon, RECT rc, COLORREF color)
        {
            // Tabler SVG is the primary icon source. Keep the legacy GDI drawing below
            // only as a fallback if Direct2D SVG initialization or asset lookup fails.
            if (DrawEditorSvgIcon(dc, static_cast<EditorIconId>(static_cast<int>(icon)), rc, color))
                return;
            if (icon == Icon::None) return;
            const int cx = static_cast<int>((rc.left + rc.right) / 2);
            const int cy = static_cast<int>((rc.top + rc.bottom) / 2);
            const int l = cx - 7, r = cx + 7, t = cy - 7, b = cy + 7;
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HBRUSH brush = CreateSolidBrush(color);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

            switch (icon)
            {
            case Icon::Document:
                Rectangle(dc, l + 2, t, r - 2, b);
                Line(dc, r - 6, t, r - 2, t + 4, color);
                break;
            case Icon::Folder:
                MoveToEx(dc, l, t + 4, nullptr); LineTo(dc, l + 5, t + 4); LineTo(dc, l + 7, t + 1);
                LineTo(dc, r, t + 1); LineTo(dc, r, b - 1); LineTo(dc, l, b - 1); LineTo(dc, l, t + 4);
                break;
            case Icon::Save:
                Rectangle(dc, l, t, r, b); Rectangle(dc, l + 3, t + 2, r - 3, t + 6); Rectangle(dc, l + 3, cy + 1, r - 3, b - 2);
                break;
            case Icon::Undo:
            case Icon::Redo:
            {
                Arc(dc, l, t + 1, r, b + 3, l, cy, r, cy);
                POINT p[3]{};
                if (icon == Icon::Undo) { p[0] = { l, cy }; p[1] = { l + 5, cy - 4 }; p[2] = { l + 5, cy + 4 }; }
                else { p[0] = { r, cy }; p[1] = { r - 5, cy - 4 }; p[2] = { r - 5, cy + 4 }; }
                SelectObject(dc, brush); Polygon(dc, p, 3); SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                break;
            }
            case Icon::Cursor:
            {
                POINT p[] = { {l + 1,t}, {l + 2,b - 1}, {l + 6,b - 5}, {l + 9,b}, {l + 12,b - 2}, {l + 8,b - 7}, {r,t + 7} };
                Polyline(dc, p, static_cast<int>(std::size(p)));
                break;
            }
            case Icon::Move:
                Line(dc, cx, t, cx, b, color); Line(dc, l, cy, r, cy, color);
                Line(dc, cx, t, cx - 3, t + 4, color); Line(dc, cx, t, cx + 3, t + 4, color);
                Line(dc, r, cy, r - 4, cy - 3, color); Line(dc, r, cy, r - 4, cy + 3, color);
                break;
            case Icon::Rotate:
                Arc(dc, l, t, r, b, r, cy, cx, t); Arc(dc, l, t, r, b, l, cy, cx, b);
                Line(dc, r - 1, cy, r - 5, cy - 3, color); Line(dc, r - 1, cy, r - 5, cy + 2, color);
                break;
            case Icon::Scale:
                Rectangle(dc, l + 1, cy - 1, cx + 1, b - 1); Rectangle(dc, cx + 2, t + 1, r - 1, cy - 2); Line(dc, cx, cy, r - 2, t + 2, color);
                break;
            case Icon::Play:
            {
                POINT p[3] = { {l + 3,t + 1}, {r - 1,cy}, {l + 3,b - 1} };
                SelectObject(dc, brush); Polygon(dc, p, 3); SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                break;
            }
            case Icon::Stop:
                SelectObject(dc, brush); Rectangle(dc, l + 3, t + 3, r - 3, b - 3); SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                break;
            case Icon::Cube:
            case Icon::Mesh:
                Rectangle(dc, l + 1, t + 2, r - 1, b - 1); Line(dc, l + 1, t + 2, r - 1, b - 1, color); Line(dc, r - 1, t + 2, l + 1, b - 1, color);
                break;
            case Icon::List:
                for (int y = t + 2; y <= b - 2; y += 5) { SelectObject(dc, brush); Rectangle(dc, l, y, l + 2, y + 2); SelectObject(dc, GetStockObject(HOLLOW_BRUSH)); Line(dc, l + 5, y + 1, r, y + 1, color); }
                break;
            case Icon::Grid:
                for (int yy = 0; yy < 2; ++yy) for (int xx = 0; xx < 2; ++xx) Rectangle(dc, l + xx * 8, t + yy * 8, l + 6 + xx * 8, t + 6 + yy * 8);
                break;
            case Icon::Settings:
                Ellipse(dc, cx - 5, cy - 5, cx + 5, cy + 5); Ellipse(dc, cx - 1, cy - 1, cx + 1, cy + 1);
                Line(dc, cx, t, cx, t + 3, color); Line(dc, cx, b - 3, cx, b, color); Line(dc, l, cy, l + 3, cy, color); Line(dc, r - 3, cy, r, cy, color);
                break;
            case Icon::Hierarchy:
                Rectangle(dc, l, t + 1, l + 4, t + 5); Rectangle(dc, r - 4, t + 1, r, t + 5); Rectangle(dc, cx - 2, b - 4, cx + 2, b);
                Line(dc, l + 2, t + 5, l + 2, cy, color); Line(dc, r - 2, t + 5, r - 2, cy, color); Line(dc, l + 2, cy, r - 2, cy, color); Line(dc, cx, cy, cx, b - 4, color);
                break;
            case Icon::Viewport:
                Rectangle(dc, l, t + 1, r, b - 2); Line(dc, cx - 4, b, cx + 4, b, color);
                break;
            case Icon::Inspector:
                for (int y = t + 2; y <= b - 2; y += 5) Line(dc, l, y, r, y, color);
                break;
            case Icon::Console:
                Rectangle(dc, l, t + 1, r, b - 1); Line(dc, l + 3, t + 4, l + 6, t + 7, color); Line(dc, l + 6, t + 7, l + 3, t + 10, color); Line(dc, l + 8, t + 10, r - 3, t + 10, color);
                break;
            case Icon::World:
                Ellipse(dc, l + 1, t + 1, r - 1, b - 1); Line(dc, cx, t + 1, cx, b - 1, color); Line(dc, l + 2, cy, r - 2, cy, color);
                break;
            case Icon::Camera:
                Rectangle(dc, l, t + 3, r - 4, b - 2); Ellipse(dc, cx - 3, cy - 3, cx + 3, cy + 3); Line(dc, r - 4, t + 6, r, t + 3, color); Line(dc, r, t + 3, r, b - 3, color);
                break;
            case Icon::Texture:
                Rectangle(dc, l, t, r, b); Line(dc, l, t, r, b, Blend(color, RGB(0,0,0), 45)); Line(dc, r, t, l, b, Blend(color, RGB(0,0,0), 45));
                break;
            case Icon::Material:
                SelectObject(dc, brush); Ellipse(dc, l + 1, t + 1, r - 1, b - 1); SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                break;
            case Icon::Text:
            case Icon::Metadata:
                Rectangle(dc, l + 2, t, r - 2, b); Line(dc, l + 5, t + 4, r - 5, t + 4, color); Line(dc, l + 5, t + 8, r - 5, t + 8, color);
                break;
            default:
                break;
            }

            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        }

        LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<ButtonState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                auto* init = reinterpret_cast<ButtonInit*>(cs->lpCreateParams);
                auto* created = new ButtonState{};
                if (init) { created->icon = init->icon; created->kind = init->kind; }
                if (cs->lpszName) created->label = cs->lpszName;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_NCDESTROY:
                delete state; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); return 0;
            case WM_SETFONT:
                if (state) state->font = reinterpret_cast<HFONT>(wParam);
                if (lParam) InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_NOC_V3_ACTIVE:
                if (state) { state->active = wParam != 0; InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            case WM_MOUSEMOVE:
                if (state && !state->hover) { state->hover = true; TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t); InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            case WM_MOUSELEAVE:
                if (state) { state->hover = false; state->pressed = false; InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            case WM_LBUTTONDOWN:
                if (state) { state->pressed = true; SetCapture(hwnd); SetFocus(hwnd); InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            case WM_LBUTTONUP:
                if (state)
                {
                    const bool click = state->pressed;
                    state->pressed = false;
                    ReleaseCapture();
                    RECT rc{}; GetClientRect(hwnd, &rc);
                    POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    if (click && PtInRect(&rc, p)) SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_KEYDOWN:
                if (wParam == VK_SPACE || wParam == VK_RETURN) { SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), reinterpret_cast<LPARAM>(hwnd)); return 0; }
                break;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc);
                const auto& c = EditorTheme::Colors();
                COLORREF bg = c.buttonBg;
                COLORREF border = Blend(c.buttonBg, c.border, 38);
                COLORREF fg = c.textPrimary;
                if (state)
                {
                    if (state->kind == ButtonKind::Menu) { bg = state->hover || state->pressed ? c.panelBgAlt : c.toolbarBg; border = bg; }
                    else if (state->active || state->kind == ButtonKind::Primary) { bg = state->pressed ? Blend(c.accent, c.windowBg, 22) : state->hover ? c.accentHover : c.accent; border = bg; fg = RGB(239,246,255); }
                    else if (state->kind == ButtonKind::Success) { bg = state->hover ? Blend(c.buttonBg, c.success, 10) : c.buttonBg; border = state->hover ? Blend(c.border, c.success, 24) : Blend(c.buttonBg, c.border, 38); fg = c.success; }
                    else if (state->hover) { bg = c.buttonHover; border = Blend(c.border, c.textMuted, 16); }
                    if (state->pressed) bg = Blend(bg, c.windowBg, 20);
                }
                Fill(dc, rc, c.toolbarBg);
                RECT box = rc; InflateRect(&box, -1, -1); RoundBox(dc, box, bg, border, 5);
                if (state)
                {
                    const bool iconOnly = state->label.empty() || state->kind == ButtonKind::IconOnly;
                    const int y = static_cast<int>(rc.top) + (static_cast<int>(rc.bottom - rc.top) - 16) / 2;
                    RECT iconRc{};
                    if (iconOnly)
                    {
                        const int cx = static_cast<int>((rc.left + rc.right) / 2);
                        iconRc = { cx - 8, y, cx + 8, y + 16 };
                    }
                    else iconRc = { rc.left + 10, y, rc.left + 26, y + 16 };
                    DrawIcon(dc, state->icon, iconRc, state->active ? RGB(244,248,255) : fg);
                    if (!iconOnly)
                    {
                        RECT tr = rc; tr.left = state->icon == Icon::None ? rc.left + 10 : rc.left + 33; tr.right -= 9;
                        DrawTextUi(dc, state->label.c_str(), tr, fg, state->font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                    if (GetFocus() == hwnd && state->kind != ButtonKind::Menu)
                    {
                        RECT focus = box; InflateRect(&focus, -2, -2);
                        HPEN p = CreatePen(PS_SOLID, 1, Blend(c.accent, c.border, 52));
                        HGDIOBJ oldPen = SelectObject(dc, p); HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                        RoundRect(dc, focus.left, focus.top, focus.right, focus.bottom, 4, 4);
                        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(p);
                    }
                }
                EndPaint(hwnd, &ps); return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        LRESULT CALLBACK HeaderProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<HeaderState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE:
            {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                auto* init = reinterpret_cast<HeaderInit*>(cs->lpCreateParams);
                auto* created = new HeaderState{};
                if (init) created->icon = init->icon;
                if (cs->lpszName) created->title = cs->lpszName;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
                return TRUE;
            }
            case WM_NCDESTROY: delete state; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); return 0;
            case WM_SETFONT: if (state) state->font = reinterpret_cast<HFONT>(wParam); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc); const auto& c = EditorTheme::Colors();
                Fill(dc, rc, c.panelBgAlt); Line(dc, 0, static_cast<int>(rc.bottom) - 1, static_cast<int>(rc.right), static_cast<int>(rc.bottom) - 1, c.border);
                if (state)
                {
                    RECT iconRc{ 10, (static_cast<int>(rc.bottom) - 15) / 2, 25, (static_cast<int>(rc.bottom) - 15) / 2 + 15 };
                    DrawIcon(dc, state->icon, iconRc, c.accent);
                    RECT tr = rc; tr.left = 32; DrawTextUi(dc, state->title.c_str(), tr, c.textPrimary, state->font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                EndPaint(hwnd, &ps); return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        std::vector<int> VisibleTree(const TreeState& state)
        {
            std::vector<int> out;
            std::vector<bool> expanded(16, true);
            for (int i = 0; i < static_cast<int>(state.items.size()); ++i)
            {
                const auto& item = state.items[i];
                bool visible = true;
                for (int d = 0; d < item.depth && d < static_cast<int>(expanded.size()); ++d) if (!expanded[d]) { visible = false; break; }
                if (visible) out.push_back(i);
                if (item.depth < static_cast<int>(expanded.size()))
                {
                    expanded[item.depth] = !item.expandable || item.expanded;
                    for (int d = item.depth + 1; d < static_cast<int>(expanded.size()); ++d) expanded[d] = true;
                }
            }
            return out;
        }

        void DrawSlimThumb(HDC dc, int totalRows, int pageRows, int firstRow, const RECT& rc)
        {
            if (totalRows <= pageRows || pageRows <= 0) return;
            const auto& c = EditorTheme::Colors();
            const int trackH = (std::max)(1, static_cast<int>(rc.bottom - rc.top) - 8);
            const int thumbH = (std::clamp)(trackH * pageRows / (std::max)(1, totalRows), 24, trackH);
            const int maxFirst = (std::max)(1, totalRows - pageRows);
            const int top = static_cast<int>(rc.top) + 4 + (trackH - thumbH) * firstRow / maxFirst;
            RECT thumb{ rc.right - 7, top, rc.right - 3, top + thumbH };
            HBRUSH brush = CreateSolidBrush(Blend(c.border, c.textMuted, 30));
            HGDIOBJ oldBrush = SelectObject(dc, brush); HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 4, 4);
            SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
        }

        LRESULT CALLBACK TreeProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<TreeState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            constexpr int rowH = 24;
            switch (msg)
            {
            case WM_NCCREATE: SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new TreeState{})); return TRUE;
            case WM_NCDESTROY: delete state; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); return 0;
            case WM_SETFONT: if (state) state->font = reinterpret_cast<HFONT>(wParam); return 0;
            case WM_MOUSEWHEEL:
                if (state)
                {
                    RECT rc{}; GetClientRect(hwnd, &rc); const auto visible = VisibleTree(*state);
                    const int page = (std::max)(1, static_cast<int>(rc.bottom) / rowH);
                    const int maxFirst = (std::max)(0, static_cast<int>(visible.size()) - page);
                    state->firstRow = (std::clamp)(state->firstRow - GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 3, 0, maxFirst);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (state)
                {
                    const auto visible = VisibleTree(*state);
                    const int visibleRow = state->firstRow + GET_Y_LPARAM(lParam) / rowH;
                    const int hover = visibleRow >= 0 && visibleRow < static_cast<int>(visible.size()) ? visible[visibleRow] : -1;
                    if (hover != state->hover) { state->hover = hover; InvalidateRect(hwnd, nullptr, FALSE); }
                    TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t);
                }
                return 0;
            case WM_MOUSELEAVE: if (state) { state->hover = -1; InvalidateRect(hwnd, nullptr, FALSE); } return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    const auto visible = VisibleTree(*state);
                    const int visibleRow = state->firstRow + GET_Y_LPARAM(lParam) / rowH;
                    if (visibleRow >= 0 && visibleRow < static_cast<int>(visible.size()))
                    {
                        const int index = visible[visibleRow]; state->selected = index;
                        const int arrowX = 9 + state->items[index].depth * 16;
                        if (state->items[index].expandable && GET_X_LPARAM(lParam) >= arrowX && GET_X_LPARAM(lParam) <= arrowX + 14) state->items[index].expanded = !state->items[index].expanded;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            case WM_ERASEBKGND: return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc); const auto& c = EditorTheme::Colors(); Fill(dc, rc, c.panelBg);
                if (state)
                {
                    const auto visible = VisibleTree(*state);
                    const int page = (std::max)(1, static_cast<int>(rc.bottom) / rowH);
                    int y = 0;
                    for (int v = state->firstRow; v < static_cast<int>(visible.size()) && y < rc.bottom; ++v, y += rowH)
                    {
                        const int index = visible[v]; const auto& item = state->items[index];
                        RECT row{ 0, y, rc.right - 10, y + rowH };
                        if (index == state->selected) Fill(dc, row, Blend(c.panelBg, c.accent, 35)); else if (index == state->hover) Fill(dc, row, c.panelBgAlt);
                        const int arrowX = 9 + item.depth * 16;
                        if (item.expandable)
                        {
                            POINT p[3]{};
                            if (item.expanded) { p[0] = { arrowX + 2,y + 9 }; p[1] = { arrowX + 10,y + 9 }; p[2] = { arrowX + 6,y + 13 }; }
                            else { p[0] = { arrowX + 4,y + 7 }; p[1] = { arrowX + 4,y + 15 }; p[2] = { arrowX + 9,y + 11 }; }
                            HBRUSH brush = CreateSolidBrush(c.textMuted); HGDIOBJ oldBrush = SelectObject(dc, brush); HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                            Polygon(dc, p, 3); SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
                        }
                        RECT iconRc{ arrowX + 15, y + 5, arrowX + 29, y + 19 }; DrawIcon(dc, item.icon, iconRc, index == state->selected ? c.textPrimary : c.textMuted);
                        RECT textRc{ arrowX + 34, y, row.right - 4, y + rowH }; DrawTextUi(dc, item.text.c_str(), textRc, c.textPrimary, state->font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                    DrawSlimThumb(dc, static_cast<int>(visible.size()), page, state->firstRow, rc);
                }
                EndPaint(hwnd, &ps); return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        LRESULT CALLBACK TableProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            auto* state = reinterpret_cast<TableState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            constexpr int headerH = 26, rowH = 25;
            switch (msg)
            {
            case WM_NCCREATE: SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new TableState{})); return TRUE;
            case WM_NCDESTROY: delete state; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); return 0;
            case WM_SETFONT: if (state) state->font = reinterpret_cast<HFONT>(wParam); return 0;
            case WM_MOUSEWHEEL:
                if (state)
                {
                    RECT rc{}; GetClientRect(hwnd, &rc); const int page = (std::max)(1, (static_cast<int>(rc.bottom) - headerH) / rowH);
                    const int maxFirst = (std::max)(0, static_cast<int>(state->rows.size()) - page);
                    state->firstRow = (std::clamp)(state->firstRow - GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 3, 0, maxFirst); InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (state)
                {
                    const int y = GET_Y_LPARAM(lParam); int hover = -1;
                    if (y >= headerH) { const int row = state->firstRow + (y - headerH) / rowH; if (row >= 0 && row < static_cast<int>(state->rows.size())) hover = row; }
                    if (hover != state->hover) { state->hover = hover; InvalidateRect(hwnd, nullptr, FALSE); }
                    TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t);
                }
                return 0;
            case WM_MOUSELEAVE: if (state) { state->hover = -1; InvalidateRect(hwnd, nullptr, FALSE); } return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    const int y = GET_Y_LPARAM(lParam);
                    if (y >= headerH) { const int row = state->firstRow + (y - headerH) / rowH; if (row >= 0 && row < static_cast<int>(state->rows.size())) { state->selected = row; InvalidateRect(hwnd, nullptr, FALSE); } }
                }
                return 0;
            case WM_ERASEBKGND: return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc); const auto& c = EditorTheme::Colors(); Fill(dc, rc, c.inputBg);
                RECT header{ 0,0,rc.right,headerH }; Fill(dc, header, c.panelBgAlt); Line(dc, 0, headerH - 1, static_cast<int>(rc.right), headerH - 1, c.border);
                const int usable = (std::max)(120, static_cast<int>(rc.right) - 9); const int typeW = (std::clamp)(usable / 3, 86, 145); const int assetW = usable - typeW;
                RECT assetHeader{ 34,0,assetW - 5,headerH }; RECT typeHeader{ assetW + 8,0,usable - 6,headerH };
                DrawTextUi(dc, L"Asset", assetHeader, c.textMuted, state ? state->font : nullptr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextUi(dc, L"Type", typeHeader, c.textMuted, state ? state->font : nullptr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                if (state)
                {
                    const int page = (std::max)(1, (static_cast<int>(rc.bottom) - headerH) / rowH);
                    if (state->rows.empty()) { RECT empty = rc; empty.top = headerH; DrawTextUi(dc, L"No assets in this folder", empty, c.textMuted, state->font, DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
                    else
                    {
                        int y = headerH;
                        for (int row = state->firstRow; row < static_cast<int>(state->rows.size()) && y < rc.bottom; ++row, y += rowH)
                        {
                            RECT rowRc{ 0,y,usable,(std::min)(static_cast<int>(rc.bottom), y + rowH) };
                            if (row == state->selected) Fill(dc, rowRc, Blend(c.inputBg, c.accent, 24)); else if (row == state->hover) Fill(dc, rowRc, Blend(c.inputBg, c.panelBgAlt, 55));
                            RECT iconRc{ 11,y + 5,25,y + 19 }; DrawIcon(dc, state->rows[row].icon, iconRc, row == state->selected ? c.textPrimary : c.textMuted);
                            RECT assetRc{ 34,y,assetW - 5,y + rowH }; RECT typeRc{ assetW + 8,y,usable - 6,y + rowH };
                            DrawTextUi(dc, state->rows[row].asset.c_str(), assetRc, c.textPrimary, state->font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                            DrawTextUi(dc, state->rows[row].type.c_str(), typeRc, c.textMuted, state->font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                        }
                        DrawSlimThumb(dc, static_cast<int>(state->rows.size()), page, state->firstRow, rc);
                    }
                }
                EndPaint(hwnd, &ps); return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        int ScrollMax(const ScrollState& state) { return (std::max)(state.minimum, state.maximum - (std::max)(1, state.page)); }

        RECT ScrollThumb(HWND hwnd, const ScrollState& state)
        {
            RECT rc{}; GetClientRect(hwnd, &rc); const int height = (std::max)(1, static_cast<int>(rc.bottom));
            const int total = (std::max)(1, state.maximum - state.minimum); const int page = (std::max)(1, state.page);
            const int thumbH = (std::clamp)(height * page / (std::max)(page, total), 24, height);
            const int travel = (std::max)(0, height - thumbH); const int maxPos = ScrollMax(state); int top = 0;
            if (maxPos > state.minimum && travel > 0) top = travel * (state.position - state.minimum) / (maxPos - state.minimum);
            return RECT{ 1, top + 1, (std::max)(2, static_cast<int>(rc.right) - 1), (std::min)(height - 1, top + thumbH - 1) };
        }

        void ScrollNotify(HWND hwnd, ScrollState& state, int position)
        {
            state.position = (std::clamp)(position, state.minimum, ScrollMax(state));
            SendMessageW(GetParent(hwnd), WM_NOC_V3_SCROLL, static_cast<WPARAM>(GetDlgCtrlID(hwnd)), static_cast<LPARAM>(state.position));
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        LRESULT CALLBACK ScrollProc(HWND hwnd, UINT msg, WPARAM, LPARAM lParam)
        {
            auto* state = reinterpret_cast<ScrollState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (msg)
            {
            case WM_NCCREATE: SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new ScrollState{})); return TRUE;
            case WM_NCDESTROY: delete state; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); return 0;
            case WM_MOUSEMOVE:
                if (state)
                {
                    POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; RECT thumb = ScrollThumb(hwnd, *state); state->thumbHover = PtInRect(&thumb, p) != FALSE;
                    if (state->dragging)
                    {
                        RECT rc{}; GetClientRect(hwnd, &rc); const int thumbH = static_cast<int>(thumb.bottom - thumb.top); const int travel = (std::max)(1, static_cast<int>(rc.bottom) - thumbH);
                        const int y = (std::clamp)(static_cast<int>(p.y) - state->dragOffset, 0, travel); const int maxPos = ScrollMax(*state);
                        ScrollNotify(hwnd, *state, state->minimum + y * (maxPos - state->minimum) / travel);
                    }
                    TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t); InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSELEAVE: if (state) { state->thumbHover = false; InvalidateRect(hwnd, nullptr, FALSE); } return 0;
            case WM_LBUTTONDOWN:
                if (state)
                {
                    POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; RECT thumb = ScrollThumb(hwnd, *state);
                    if (PtInRect(&thumb, p)) { state->dragging = true; state->dragOffset = static_cast<int>(p.y - thumb.top); SetCapture(hwnd); }
                    else ScrollNotify(hwnd, *state, state->position + (p.y < thumb.top ? -state->page : state->page));
                }
                return 0;
            case WM_LBUTTONUP: if (state && state->dragging) { state->dragging = false; ReleaseCapture(); InvalidateRect(hwnd, nullptr, FALSE); } return 0;
            case WM_ERASEBKGND: return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc); const auto& c = EditorTheme::Colors(); Fill(dc, rc, c.inputBg);
                if (state && ScrollMax(*state) > state->minimum)
                {
                    RECT thumb = ScrollThumb(hwnd, *state); const COLORREF col = state->dragging ? Blend(c.textMuted, c.accent, 42) : state->thumbHover ? Blend(c.border, c.textMuted, 45) : Blend(c.border, c.textMuted, 24);
                    HBRUSH brush = CreateSolidBrush(col); HGDIOBJ oldBrush = SelectObject(dc, brush); HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                    RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 7, 7); SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
                }
                EndPaint(hwnd, &ps); return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, 0, lParam);
        }

        bool RegisterClass(HINSTANCE instance, const wchar_t* name, WNDPROC proc, HCURSOR cursor)
        {
            WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.hInstance = instance; wc.lpfnWndProc = proc; wc.lpszClassName = name; wc.hCursor = cursor; wc.hbrBackground = nullptr; wc.style = CS_HREDRAW | CS_VREDRAW;
            if (RegisterClassExW(&wc) != 0) return true;
            return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }

        bool RegisterV3Classes(HINSTANCE instance)
        {
            return RegisterClass(instance, kButtonClass, ButtonProc, LoadCursorW(nullptr, IDC_HAND)) &&
                RegisterClass(instance, kHeaderClass, HeaderProc, LoadCursorW(nullptr, IDC_ARROW)) &&
                RegisterClass(instance, kTreeClass, TreeProc, LoadCursorW(nullptr, IDC_ARROW)) &&
                RegisterClass(instance, kTableClass, TableProc, LoadCursorW(nullptr, IDC_ARROW)) &&
                RegisterClass(instance, kScrollClass, ScrollProc, LoadCursorW(nullptr, IDC_ARROW));
        }

        HWND MakeButton(HWND parent, int id, const wchar_t* text, Icon icon, ButtonKind kind, HFONT font)
        {
            ButtonInit init{ icon, kind };
            HWND h = CreateWindowExW(0, kButtonClass, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0,0,0,0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), &init);
            if (h && font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            return h;
        }

        HWND MakeHeader(HWND parent, const wchar_t* text, Icon icon, HFONT font)
        {
            HeaderInit init{ icon };
            HWND h = CreateWindowExW(0, kHeaderClass, text, WS_CHILD | WS_VISIBLE, 0,0,0,0, parent, nullptr, GetModuleHandleW(nullptr), &init);
            if (h && font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            return h;
        }

        HWND MakeTree(HWND parent, int id, HFONT font)
        {
            HWND h = CreateWindowExW(0, kTreeClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0,0,0,0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
            if (h && font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE); return h;
        }

        HWND MakeTable(HWND parent, int id, HFONT font)
        {
            HWND h = CreateWindowExW(0, kTableClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0,0,0,0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
            if (h && font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE); return h;
        }

        HWND MakeScroll(HWND parent, int id)
        {
            return CreateWindowExW(0, kScrollClass, L"", WS_CHILD | WS_VISIBLE, 0,0,0,0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        }

        void ButtonActive(HWND h, bool active) { if (h) SendMessageW(h, WM_NOC_V3_ACTIVE, active ? TRUE : FALSE, 0); }
        void TreeClear(HWND h) { auto* s = h ? reinterpret_cast<TreeState*>(GetWindowLongPtrW(h, GWLP_USERDATA)) : nullptr; if (!s) return; s->items.clear(); s->firstRow = 0; s->selected = -1; s->hover = -1; InvalidateRect(h, nullptr, FALSE); }
        void TreeAdd(HWND h, std::wstring text, int depth, Icon icon, bool expandable = false, bool expanded = true) { auto* s = h ? reinterpret_cast<TreeState*>(GetWindowLongPtrW(h, GWLP_USERDATA)) : nullptr; if (!s) return; s->items.push_back({ std::move(text), depth, icon, expandable, expanded }); if (s->selected < 0) s->selected = 0; InvalidateRect(h, nullptr, FALSE); }
        void TableClear(HWND h) { auto* s = h ? reinterpret_cast<TableState*>(GetWindowLongPtrW(h, GWLP_USERDATA)) : nullptr; if (!s) return; s->rows.clear(); s->firstRow = 0; s->selected = -1; s->hover = -1; InvalidateRect(h, nullptr, FALSE); }
        void TableAdd(HWND h, std::wstring asset, std::wstring type, Icon icon) { auto* s = h ? reinterpret_cast<TableState*>(GetWindowLongPtrW(h, GWLP_USERDATA)) : nullptr; if (!s) return; s->rows.push_back({ std::move(asset), std::move(type), icon }); InvalidateRect(h, nullptr, FALSE); }
        void SetScroll(HWND h, int maximum, int page, int pos) { auto* s = h ? reinterpret_cast<ScrollState*>(GetWindowLongPtrW(h, GWLP_USERDATA)) : nullptr; if (!s) return; s->minimum = 0; s->maximum = maximum; s->page = (std::max)(1, page); s->position = (std::clamp)(pos, 0, ScrollMax(*s)); InvalidateRect(h, nullptr, FALSE); }

        Icon IconForType(const std::wstring& type)
        {
            if (type == L"Folder") return Icon::Folder; if (type == L"Mesh") return Icon::Mesh; if (type == L"Texture") return Icon::Texture;
            if (type == L"Material") return Icon::Material; if (type == L"Text") return Icon::Text; if (type == L"Metadata") return Icon::Metadata;
            return Icon::Cube;
        }

        std::wstring AssetTypeForPath(const std::filesystem::path& path, bool directory)
        {
            if (directory) return L"Folder";
            std::wstring ext = path.extension().wstring(); std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".obj" || ext == L".nmsh") return L"Mesh";
            if (ext == L".bmp" || ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".ntx") return L"Texture";
            if (ext == L".txt") return L"Text"; if (ext == L".json") return L"Metadata"; if (ext == L".nmat") return L"Material"; return L"Asset";
        }

        void RichColor(HWND edit, COLORREF color)
        {
            CHARFORMAT2W f{}; f.cbSize = sizeof(f); f.dwMask = CFM_COLOR; f.crTextColor = color;
            SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&f));
        }

        void RichAppend(HWND edit, const std::wstring& text, COLORREF color)
        {
            const LRESULT len = SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0); SendMessageW(edit, EM_SETSEL, len, len); RichColor(edit, color); SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
        }
    }

    bool EditorShellV3::Init(noc::Engine& engine, noc::WinWindow& window)
    {
        engine_ = &engine; window_ = &window; hwnd_ = static_cast<HWND>(window.Handle()); if (!hwnd_) return false;
        if (!RegisterV3Classes(GetModuleHandleW(nullptr))) return false;
        constexpr DWORD kDark = 20; BOOL dark = TRUE; DwmSetWindowAttribute(hwnd_, kDark, &dark, sizeof(dark));

        uiFont_ = MakeFont(12, FW_NORMAL, L"Segoe UI Variable Text"); uiBold_ = MakeFont(12, FW_SEMIBOLD, L"Segoe UI Variable Text");
        smallFont_ = MakeFont(11, FW_NORMAL, L"Segoe UI Variable Text"); consoleFont_ = MakeFont(11, FW_NORMAL, L"Cascadia Mono"); brandFont_ = MakeFont(42, FW_SEMIBOLD, L"Segoe UI Variable Display");
        const auto& c = EditorTheme::Colors(); windowBrush_ = CreateSolidBrush(c.windowBg); editBrush_ = CreateSolidBrush(c.inputBg);

        std::wstring configured = L"Data";
        const auto& cfg = engine.Config();
        if (cfg.contentRoot && cfg.contentRoot[0] != '\0') configured = Utf8ToWide_(cfg.contentRoot);
        contentRoot_ = ResolveContentRoot_(configured);

        CreateChrome_(); CreateToolbar_(); CreatePanels_(); PopulateScene_(); PopulateContent_(); window.SetMessageSink(this);
        RECT rc{}; GetClientRect(hwnd_, &rc); Layout_(static_cast<int>(rc.right - rc.left), static_cast<int>(rc.bottom - rc.top));
        RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        AppendConsole_(L"Nocturne Editor initialized."); AppendConsole_(L"Phase 13: UI Fidelity Pass 3 active."); AppendConsole_(L"Vector icons, toolbar grouping and Content Browser composition enabled.");
        UpdateStatus_(); NOC_LOG_INFO("Editor", "Nocturne Editor UI Fidelity Pass 3 initialized"); return true;
    }

    void EditorShellV3::Shutdown()
    {
        if (window_) window_->SetMessageSink(nullptr); if (fileMenu_) DestroyMenu(fileMenu_); if (buildMenu_) DestroyMenu(buildMenu_); fileMenu_ = buildMenu_ = nullptr;
        if (uiFont_) DeleteObject(uiFont_); if (uiBold_) DeleteObject(uiBold_); if (smallFont_) DeleteObject(smallFont_); if (consoleFont_) DeleteObject(consoleFont_); if (brandFont_) DeleteObject(brandFont_);
        if (windowBrush_) DeleteObject(windowBrush_); if (editBrush_) DeleteObject(editBrush_);
        uiFont_ = uiBold_ = smallFont_ = consoleFont_ = brandFont_ = nullptr; windowBrush_ = editBrush_ = nullptr; engine_ = nullptr; window_ = nullptr; hwnd_ = nullptr;
    }

    void EditorShellV3::CreateChrome_()
    {
        menuBand_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0,0,0,0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        const struct M { const wchar_t* text; int id; } defs[] = { {L"File",IdMenuFile},{L"Edit",IdMenuEdit},{L"Window",IdMenuWindow},{L"Tools",IdMenuTools},{L"Build",IdMenuBuild},{L"Select",IdMenuSelect},{L"Actor",IdMenuActor},{L"Help",IdMenuHelp} };
        for (const auto& d : defs) menuButtons_.push_back(MakeButton(hwnd_, d.id, d.text, Icon::None, ButtonKind::Menu, uiFont_));
        fileMenu_ = CreatePopupMenu(); AppendMenuW(fileMenu_, MF_STRING, IdToolbarNew, L"New Scene"); AppendMenuW(fileMenu_, MF_STRING, IdToolbarOpen, L"Open Scene..."); AppendMenuW(fileMenu_, MF_STRING, IdToolbarSave, L"Save Scene"); AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr); AppendMenuW(fileMenu_, MF_STRING, IDCANCEL, L"Exit");
        buildMenu_ = CreatePopupMenu(); AppendMenuW(buildMenu_, MF_STRING, IdToolbarBuild, L"Build Content"); AppendMenuW(buildMenu_, MF_STRING, IdToolbarPlay, L"Play");
    }

    void EditorShellV3::CreateToolbar_()
    {
        toolbarBand_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0,0,0,0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        const struct T { const wchar_t* text; int id; Icon icon; ButtonKind kind; } defs[] = {
            {L"New",IdToolbarNew,Icon::Document,ButtonKind::Neutral},{L"Open",IdToolbarOpen,Icon::Folder,ButtonKind::Neutral},{L"Save",IdToolbarSave,Icon::Save,ButtonKind::Neutral},
            {L"Undo",IdToolbarUndo,Icon::Undo,ButtonKind::Neutral},{L"Redo",IdToolbarRedo,Icon::Redo,ButtonKind::Neutral},
            {L"Select",IdToolbarSelect,Icon::Cursor,ButtonKind::Tool},{L"Move",IdToolbarMove,Icon::Move,ButtonKind::Tool},{L"Rotate",IdToolbarRotate,Icon::Rotate,ButtonKind::Tool},{L"Scale",IdToolbarScale,Icon::Scale,ButtonKind::Tool},
            {L"Play",IdToolbarPlay,Icon::Play,ButtonKind::Success},{L"Stop",IdToolbarStop,Icon::Stop,ButtonKind::Neutral},{L"Build",IdToolbarBuild,Icon::Cube,ButtonKind::Neutral} };
        for (const auto& d : defs) { HWND h = MakeButton(hwnd_, d.id, d.text, d.icon, d.kind, uiBold_); if (d.id == activeToolId_) ButtonActive(h, true); toolbarButtons_.push_back(h); }
    }

    void EditorShellV3::CreatePanels_()
    {
        auto makeBody = [&](int id = 0) { return CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0,0,0,0, hwnd_, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr, GetModuleHandleW(nullptr), nullptr); };
        scene_.header = MakeHeader(hwnd_, L"Scene Hierarchy", Icon::Hierarchy, uiBold_); sceneTree_ = MakeTree(hwnd_, IdSceneTree, uiFont_); scene_.body = sceneTree_;
        viewport_.header = MakeHeader(hwnd_, L"Viewport", Icon::Viewport, uiBold_); viewport_.body = makeBody(); viewportPerspective_ = MakeButton(hwnd_, IdViewportPerspective, L"Perspective", Icon::None, ButtonKind::Tool, uiFont_); viewportLit_ = MakeButton(hwnd_, IdViewportLit, L"Lit", Icon::None, ButtonKind::Neutral, uiFont_); viewportShow_ = MakeButton(hwnd_, IdViewportShow, L"Show", Icon::None, ButtonKind::Neutral, uiFont_); ButtonActive(viewportPerspective_, true);
        inspector_.header = MakeHeader(hwnd_, L"Inspector / Properties", Icon::Inspector, uiBold_); inspector_.body = makeBody(IdInspector);
        content_.header = MakeHeader(hwnd_, L"Content Browser", Icon::Folder, uiBold_); content_.body = makeBody();
        contentSearch_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,0,0,0, hwnd_, reinterpret_cast<HMENU>(IdContentSearch), GetModuleHandleW(nullptr), nullptr); SendMessageW(contentSearch_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), FALSE); SendMessageW(contentSearch_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search Assets..."));
        contentListMode_ = MakeButton(hwnd_, IdContentListMode, L"", Icon::List, ButtonKind::IconOnly, uiFont_); contentGridMode_ = MakeButton(hwnd_, IdContentGridMode, L"", Icon::Grid, ButtonKind::IconOnly, uiFont_); contentSettings_ = MakeButton(hwnd_, IdContentSettings, L"", Icon::Settings, ButtonKind::IconOnly, uiFont_); ButtonActive(contentListMode_, true);
        contentTree_ = MakeTree(hwnd_, IdContentTree, uiFont_); contentTable_ = MakeTable(hwnd_, IdContentTable, uiFont_);
        console_.header = MakeHeader(hwnd_, L"Console / Output", Icon::Console, uiBold_); console_.body = makeBody(); LoadLibraryW(L"Msftedit.dll");
        consoleEdit_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL, 0,0,0,0, hwnd_, reinterpret_cast<HMENU>(IdConsole), GetModuleHandleW(nullptr), nullptr); SendMessageW(consoleEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(consoleFont_), FALSE); SendMessageW(consoleEdit_, EM_SETBKGNDCOLOR, 0, EditorTheme::Colors().inputBg); SendMessageW(consoleEdit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8,8)); consoleScroll_ = MakeScroll(hwnd_, IdConsoleScroll);
        buildPlay_.header = MakeHeader(hwnd_, L"Build / Play", Icon::Play, uiBold_); buildPlay_.body = makeBody(); playButton_ = MakeButton(hwnd_, IdPlay, L"Play (F5)", Icon::Play, ButtonKind::Primary, uiBold_); buildButton_ = MakeButton(hwnd_, IdBuild, L"Build", Icon::Cube, ButtonKind::Neutral, uiBold_);
        status_ = makeBody(IdStatus);
    }

    void EditorShellV3::PopulateScene_()
    {
        TreeClear(sceneTree_); TreeAdd(sceneTree_, L"Scene (Runtime World)", 0, Icon::World, true, true); const uint32_t alive = engine_ ? engine_->GetWorld().AliveCount() : 0; std::wstringstream ss; ss << L"Runtime Objects: " << alive; TreeAdd(sceneTree_, ss.str(), 1, Icon::Cube); TreeAdd(sceneTree_, L"Main Camera", 1, Icon::Camera); TreeAdd(sceneTree_, L"Environment", 1, Icon::Folder);
    }

    void EditorShellV3::PopulateContent_()
    {
        TreeClear(contentTree_); TableClear(contentTable_); TreeAdd(contentTree_, L"Content", 0, Icon::Folder, true, true);
        namespace fs = std::filesystem; std::error_code ec; const fs::path root(contentRoot_);
        if (!fs::exists(root, ec)) { TreeAdd(contentTree_, L"Content root unavailable", 1, Icon::Folder); return; }
        std::vector<fs::directory_entry> entries; for (const auto& e : fs::directory_iterator(root, ec)) { if (ec) break; entries.push_back(e); }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.path().filename().wstring() < b.path().filename().wstring(); });
        for (const auto& e : entries) { const std::wstring name = e.path().filename().wstring(); const bool dir = e.is_directory(ec); const std::wstring type = AssetTypeForPath(e.path(), dir); if (dir) TreeAdd(contentTree_, name, 1, Icon::Folder); TableAdd(contentTable_, name, type, IconForType(type)); }
    }

    void EditorShellV3::Layout_(int w, int h)
    {
        if (w <= 0 || h <= 0) return;
        constexpr int menuH = 26, toolbarH = 48, statusH = 24, gap = 7, headerH = 29, buttonH = 34;
        MoveWindow(menuBand_, 0,0,w,menuH, TRUE); int menuX = 11; const int menuWidths[] = {40,40,60,46,46,52,46,42}; for (size_t i = 0; i < menuButtons_.size(); ++i) { const int bw = menuWidths[i]; MoveWindow(menuButtons_[i], menuX,1,bw,menuH-2,TRUE); menuX += bw + 2; }
        MoveWindow(toolbarBand_, 0,menuH,w,toolbarH,TRUE); int x = 12; const int widths[] = {76,80,76,80,80,90,84,88,84,86,82,86}; for (size_t i = 0; i < toolbarButtons_.size(); ++i) { const int bw = widths[i]; MoveWindow(toolbarButtons_[i], x, menuH + (toolbarH - buttonH) / 2, bw, buttonH, TRUE); x += bw + 4; if (i == 2 || i == 4 || i == 8) x += 10; }
        const int top = menuH + toolbarH + gap; const int statusY = (std::max)(top, h - statusH); const int available = (std::max)(0, statusY - top - gap); const int bottomH = (std::clamp)(available * 34 / 100, 205, 280); const int topH = (std::max)(140, available - bottomH - gap); const int bottomY = top + topH + gap; const int actualBottom = (std::max)(0, statusY - bottomY);
        const int leftW = (std::clamp)(w * 20 / 100, 250, 330); const int rightW = (std::clamp)(w * 22 / 100, 285, 355); const int centerX = gap + leftW + gap; const int centerW = (std::max)(260, w - leftW - rightW - 4 * gap); const int rightX = centerX + centerW + gap;
        auto panel = [&](Panel& p, int px, int py, int pw, int ph) { MoveWindow(p.header, px,py,pw,headerH,TRUE); MoveWindow(p.body, px,py+headerH,pw,(std::max)(0,ph-headerH),TRUE); };
        panel(scene_, gap,top,leftW,topH); panel(viewport_, centerX,top,centerW,topH); panel(inspector_, rightX,top,rightW,topH);
        const int viewportY = top + headerH; MoveWindow(viewportPerspective_, centerX+14,viewportY+10,90,30,TRUE); MoveWindow(viewportLit_, centerX+109,viewportY+10,50,30,TRUE); MoveWindow(viewportShow_, centerX+164,viewportY+10,58,30,TRUE);
        const int bottomLeft = (std::clamp)(w * 31 / 100, 345, 495); const int bottomCenterX = gap + bottomLeft + gap; const int bottomCenter = (std::max)(260, w - bottomLeft - rightW - 4 * gap); const int bottomRightX = bottomCenterX + bottomCenter + gap;
        panel(content_, gap,bottomY,bottomLeft,actualBottom); panel(console_, bottomCenterX,bottomY,bottomCenter,actualBottom); panel(buildPlay_, bottomRightX,bottomY,rightW,actualBottom);
        const int contentY = bottomY + headerH, contentH = (std::max)(0, actualBottom - headerH), pad = 9, searchH = 30, iconW = 30, iconGap = 4; const int actionsW = iconW * 3 + iconGap * 2;
        MoveWindow(contentSearch_, gap+pad,contentY+pad,(std::max)(80,bottomLeft-2*pad-actionsW-7),searchH,TRUE); const int actionX = gap + bottomLeft - pad - actionsW; MoveWindow(contentListMode_, actionX,contentY+pad,iconW,searchH,TRUE); MoveWindow(contentGridMode_, actionX+iconW+iconGap,contentY+pad,iconW,searchH,TRUE); MoveWindow(contentSettings_, actionX+(iconW+iconGap)*2,contentY+pad,iconW,searchH,TRUE);
        const int browserY = contentY + pad + searchH + 7, browserH = (std::max)(0, contentH - (2*pad + searchH + 7)); const int treeW = (std::clamp)(bottomLeft * 30 / 100, 110, 145); MoveWindow(contentTree_, gap+pad,browserY,treeW,browserH,TRUE); MoveWindow(contentTable_, gap+pad+treeW+7,browserY,(std::max)(0,bottomLeft-2*pad-treeW-7),browserH,TRUE);
        const int consoleY = bottomY + headerH, consoleH = (std::max)(0, actualBottom - headerH), scrollW = 8; MoveWindow(consoleEdit_, bottomCenterX+8,consoleY+6,(std::max)(0,bottomCenter-8-6-scrollW-3),(std::max)(0,consoleH-12),TRUE); MoveWindow(consoleScroll_, bottomCenterX+bottomCenter-scrollW-6,consoleY+7,scrollW,(std::max)(0,consoleH-14),TRUE);
        const int buttonY = bottomY + actualBottom - buttonH - 9, actionGap = 9, actionW = (std::max)(96,(rightW-24-actionGap)/2); MoveWindow(playButton_, bottomRightX+12,buttonY,actionW,buttonH,TRUE); MoveWindow(buildButton_, bottomRightX+12+actionW+actionGap,buttonY,actionW,buttonH,TRUE);
        MoveWindow(status_, 0,statusY,w,statusH,TRUE); InvalidateRect(viewport_.body,nullptr,FALSE); InvalidateRect(inspector_.body,nullptr,FALSE); InvalidateRect(buildPlay_.body,nullptr,FALSE); InvalidateRect(status_,nullptr,FALSE);
    }

    void EditorShellV3::AppendConsole_(const wchar_t* message)
    {
        if (!consoleEdit_ || !message) return; SendMessageW(consoleEdit_, EM_SETREADONLY, FALSE, 0); SYSTEMTIME st{}; GetLocalTime(&st); wchar_t ts[32]{}; swprintf_s(ts, L"[%02u:%02u:%02u] ", st.wHour,st.wMinute,st.wSecond);
        const auto& c = EditorTheme::Colors(); RichAppend(consoleEdit_, ts, c.textMuted); RichAppend(consoleEdit_, L"[Editor] ", c.accent); std::wstring msg(message), lower = msg; std::transform(lower.begin(),lower.end(),lower.begin(),::towlower); COLORREF color = c.textPrimary; if (lower.find(L"error") != std::wstring::npos || lower.find(L"failed") != std::wstring::npos) color = c.danger; else if (lower.find(L"warning") != std::wstring::npos) color = c.warning; RichAppend(consoleEdit_, msg + L"\r\n", color); SendMessageW(consoleEdit_, EM_SETREADONLY, TRUE, 0); SendMessageW(consoleEdit_, EM_SCROLLCARET, 0, 0);
        RECT rc{}; GetClientRect(consoleEdit_, &rc); const int lines = static_cast<int>(SendMessageW(consoleEdit_, EM_GETLINECOUNT, 0, 0)); const int first = static_cast<int>(SendMessageW(consoleEdit_, EM_GETFIRSTVISIBLELINE, 0, 0)); const int page = (std::max)(1, static_cast<int>(rc.bottom) / 15); SetScroll(consoleScroll_, lines, page, first);
    }

    void EditorShellV3::HandleCommand_(int id)
    {
        switch (id)
        {
        case IdToolbarNew: AppendConsole_(L"New Scene requested. Scene authoring is scheduled for Phase 16."); break;
        case IdToolbarOpen: AppendConsole_(L"Open Scene requested. Scene serialization is scheduled for Phase 17."); break;
        case IdToolbarSave: AppendConsole_(L"Save requested. Serialization remains Phase 17 scope."); break;
        case IdToolbarUndo: case IdToolbarRedo: AppendConsole_(L"Undo/Redo received; edit history arrives with scene editing."); break;
        case IdToolbarSelect: case IdToolbarMove: case IdToolbarRotate: case IdToolbarScale:
            activeToolId_ = id; for (HWND h : toolbarButtons_) { const int bid = GetDlgCtrlID(h); if (bid >= IdToolbarSelect && bid <= IdToolbarScale) ButtonActive(h, bid == activeToolId_); } AppendConsole_(L"Viewport tool selected. Interactive gizmos remain Phase 14 scope."); break;
        case IdToolbarPlay: case IdPlay: AppendConsole_(L"Play requested. Full Play-In-Editor remains Phase 27 scope."); break;
        case IdToolbarStop: AppendConsole_(L"Stop requested."); break;
        case IdToolbarBuild: case IdBuild:
            if (engine_) { AppendConsole_(L"Running asset import pass through the existing Phase 11 pipeline..."); const bool ok = engine_->Assets().ImportAll(); AppendConsole_(ok ? L"Asset import completed." : L"Asset import completed with errors. Check Log.txt."); PopulateContent_(); } break;
        case IdContentListMode: ButtonActive(contentListMode_, true); ButtonActive(contentGridMode_, false); AppendConsole_(L"Content Browser list view selected."); break;
        case IdContentGridMode: AppendConsole_(L"Grid view is a Phase 13 tooling-shell stub; list mode remains active."); break;
        case IdContentSettings: AppendConsole_(L"Content Browser settings shell selected."); break;
        case IdViewportPerspective: case IdViewportLit: case IdViewportShow: AppendConsole_(L"Viewport display control selected; rendering remains Phase 14 scope."); break;
        default: break;
        }
        PopulateScene_(); UpdateStatus_();
    }

    void EditorShellV3::ShowPopup_(int menuId, HWND anchor)
    {
        HMENU menu = nullptr; if (menuId == IdMenuFile) menu = fileMenu_; else if (menuId == IdMenuBuild) menu = buildMenu_; bool temporary = false;
        if (!menu) { menu = CreatePopupMenu(); AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Phase 13 tooling shell"); temporary = true; }
        RECT rc{}; GetWindowRect(anchor, &rc); const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, rc.left,rc.bottom+2,hwnd_,nullptr); if (command) HandleCommand_(command); if (temporary) DestroyMenu(menu);
    }

    void EditorShellV3::UpdateStatus_() { if (status_) InvalidateRect(status_, nullptr, FALSE); }

    bool EditorShellV3::OnWindowMessage(void* hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam, intptr_t& result)
    {
        const HWND native = static_cast<HWND>(hwnd); const auto& c = EditorTheme::Colors();
        switch (msg)
        {
        case WM_SIZE: Layout_(LOWORD(lParam), HIWORD(lParam)); InvalidateRect(native, nullptr, TRUE); result = 0; return false;
        case WM_ERASEBKGND: { RECT rc{}; GetClientRect(native, &rc); FillRect(reinterpret_cast<HDC>(wParam), &rc, windowBrush_); result = 1; return true; }
        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(lParam) == contentSearch_) { HDC dc = reinterpret_cast<HDC>(wParam); SetTextColor(dc, c.textPrimary); SetBkColor(dc, c.inputBg); result = reinterpret_cast<intptr_t>(editBrush_); return true; } break;
        case WM_COMMAND:
        {
            const int id = LOWORD(wParam); HWND source = reinterpret_cast<HWND>(lParam);
            if (id >= IdMenuFile && id <= IdMenuHelp) { ShowPopup_(id, source); result = 0; return true; }
            if (id == IDCANCEL) { if (window_) window_->RequestQuit(); result = 0; return true; }
            if ((id >= IdToolbarNew && id <= IdToolbarBuild) || id == IdPlay || id == IdBuild || id == IdContentListMode || id == IdContentGridMode || id == IdContentSettings || id == IdViewportPerspective || id == IdViewportLit || id == IdViewportShow) { HandleCommand_(id); result = 0; return true; }
            break;
        }
        case WM_NOC_V3_SCROLL:
            if (static_cast<int>(wParam) == IdConsoleScroll && consoleEdit_) { const int target = static_cast<int>(lParam); const int current = static_cast<int>(SendMessageW(consoleEdit_, EM_GETFIRSTVISIBLELINE, 0, 0)); SendMessageW(consoleEdit_, EM_LINESCROLL, 0, target - current); result = 0; return true; } break;
        case WM_DRAWITEM:
        {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam); if (!dis) break;
            if (dis->hwndItem == menuBand_ || dis->hwndItem == toolbarBand_) { Fill(dis->hDC, dis->rcItem, c.toolbarBg); Line(dis->hDC, 0, static_cast<int>(dis->rcItem.bottom)-1, static_cast<int>(dis->rcItem.right), static_cast<int>(dis->rcItem.bottom)-1, c.border); result = TRUE; return true; }
            if (dis->hwndItem == viewport_.body)
            {
                RECT rc = dis->rcItem; Fill(dis->hDC, rc, c.viewportBg); const int horizon = static_cast<int>(rc.top) + static_cast<int>(rc.bottom-rc.top) * 42 / 100; const COLORREF grid = Blend(c.viewportBg,c.border,38);
                for (int y = horizon; y < rc.bottom; y += 24) Line(dis->hDC, rc.left,y,rc.right,y,grid); const int cx = static_cast<int>((rc.left+rc.right)/2); for (int i=-10;i<=10;++i) Line(dis->hDC,cx,horizon,cx+i*56,rc.bottom,grid);
                RECT brand{rc.left,horizon-34,rc.right,horizon+18}; DrawTextUi(dis->hDC,L"N",brand,Blend(c.viewportBg,c.textMuted,34),brandFont_,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                RECT title{rc.left,horizon+18,rc.right,horizon+42}; DrawTextUi(dis->hDC,L"NOCTURNE VIEWPORT",title,c.textPrimary,uiBold_,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                RECT sub{rc.left+40,horizon+40,rc.right-40,horizon+62}; DrawTextUi(dis->hDC,L"Phase 14 — rendering viewport, camera navigation, selection and gizmos",sub,c.textMuted,smallFont_,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
                const int ox=static_cast<int>(rc.left)+34,oy=static_cast<int>(rc.bottom)-32; Line(dis->hDC,ox,oy,ox+28,oy,RGB(230,73,78),2); Line(dis->hDC,ox,oy,ox,oy-28,RGB(94,211,111),2);
                RECT badge{rc.right-112,rc.bottom-38,rc.right-14,rc.bottom-10}; RoundBox(dis->hDC,badge,Blend(c.viewportBg,c.panelBgAlt,60),c.border,6); DrawTextUi(dis->hDC,L"Grid: 1.0 m",badge,c.textMuted,smallFont_,DT_CENTER|DT_VCENTER|DT_SINGLELINE); result=TRUE; return true;
            }
            if (dis->hwndItem == inspector_.body)
            {
                RECT rc=dis->rcItem; Fill(dis->hDC,rc,c.panelBg); RECT title{rc.left+18,rc.top+54,rc.right-18,rc.top+78}; DrawTextUi(dis->hDC,L"No object selected",title,c.textPrimary,uiBold_,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                RECT helper{rc.left+20,rc.top+84,rc.right-20,rc.top+120}; DrawTextUi(dis->hDC,L"Select an object in the scene to inspect its properties.",helper,c.textMuted,smallFont_,DT_CENTER|DT_WORDBREAK); Line(dis->hDC,rc.left+18,rc.top+142,rc.right-18,rc.top+142,c.border);
                RECT tipsTitle{rc.left+20,rc.top+156,rc.right-20,rc.top+180}; DrawTextUi(dis->hDC,L"Tips",tipsTitle,c.textPrimary,uiBold_,DT_LEFT|DT_VCENTER|DT_SINGLELINE); const wchar_t* tips[]={L"• Select an object in Scene Hierarchy or click in the viewport.",L"• Use Select / Move / Rotate / Scale from the toolbar.",L"• Inspector components arrive with scene editing."}; int y=rc.top+185; for(const auto* tip:tips){RECT tr{rc.left+21,y,rc.right-18,y+40};DrawTextUi(dis->hDC,tip,tr,c.textMuted,smallFont_,DT_LEFT|DT_WORDBREAK);y+=52;} result=TRUE; return true;
            }
            if (dis->hwndItem == content_.body || dis->hwndItem == console_.body) { Fill(dis->hDC,dis->rcItem,c.panelBg); result=TRUE; return true; }
            if (dis->hwndItem == buildPlay_.body)
            {
                RECT rc=dis->rcItem; Fill(dis->hDC,rc,c.panelBg); RECT heading{rc.left+15,rc.top+12,rc.right-15,rc.top+34}; DrawTextUi(dis->hDC,L"Play Options",heading,c.textPrimary,uiBold_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
                struct Row{const wchar_t* label;const wchar_t* value;}rows[]={{L"Play Mode",L"Selected Viewport"},{L"Start Map",L"Current Scene"}}; int y=rc.top+43;
                for(const auto& row:rows){RECT label{rc.left+15,y,rc.left+104,y+29};DrawTextUi(dis->hDC,row.label,label,c.textMuted,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT field{rc.left+112,y+1,rc.right-15,y+28};RoundBox(dis->hDC,field,c.inputBg,c.border,5);field.left+=9;DrawTextUi(dis->hDC,row.value,field,c.textPrimary,uiFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);y+=38;}
                RECT vsync{rc.left+15,y,rc.left+104,y+29};DrawTextUi(dis->hDC,L"VSync",vsync,c.textMuted,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT toggle{rc.left+112,y+5,rc.left+150,y+24};RoundBox(dis->hDC,toggle,c.accent,c.accent,10);HBRUSH knob=CreateSolidBrush(RGB(238,245,252));HGDIOBJ oldBrush=SelectObject(dis->hDC,knob);HGDIOBJ oldPen=SelectObject(dis->hDC,GetStockObject(NULL_PEN));Ellipse(dis->hDC,toggle.right-17,toggle.top+3,toggle.right-4,toggle.bottom-3);SelectObject(dis->hDC,oldPen);SelectObject(dis->hDC,oldBrush);DeleteObject(knob);RECT enabled{rc.left+160,y,rc.right-15,y+29};DrawTextUi(dis->hDC,L"Enabled",enabled,c.textPrimary,uiFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);result=TRUE;return true;
            }
            if (dis->hwndItem == status_)
            {
                RECT rc=dis->rcItem; Fill(dis->hDC,rc,c.toolbarBg); Line(dis->hDC,0,0,static_cast<int>(rc.right),0,c.border); const int objects=engine_?static_cast<int>(engine_->GetWorld().AliveCount()):0; std::wstringstream ss;ss<<L"Objects: "<<objects;
                RECT ready{16,0,130,rc.bottom};DrawTextUi(dis->hDC,L"● Ready",ready,c.success,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT issues{138,0,250,rc.bottom};DrawTextUi(dis->hDC,L"✓ No Issues",issues,c.success,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
                RECT branch{270,0,rc.right-350,rc.bottom};DrawTextUi(dis->hDC,L"Branch: phase-13-editor-framework",branch,c.textMuted,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);const std::wstring objectText=ss.str();const int objectLeft=(std::max)(0,static_cast<int>(rc.right)-335);RECT objectRc{objectLeft,0,rc.right-220,rc.bottom};DrawTextUi(dis->hDC,objectText.c_str(),objectRc,c.textMuted,smallFont_,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT engineRc{rc.right-205,0,rc.right-52,rc.bottom};DrawTextUi(dis->hDC,L"Nocturne Engine",engineRc,c.textPrimary,smallFont_,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);RECT versionRc{rc.right-48,0,rc.right-10,rc.bottom};DrawTextUi(dis->hDC,L"v0.1.0",versionRc,c.textMuted,smallFont_,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);result=TRUE;return true;
            }
            break;
        }
        }
        return false;
    }

    std::wstring EditorShellV3::ResolveContentRoot_(const std::wstring& configured) const
    {
        namespace fs=std::filesystem; std::error_code ec; fs::path p(configured);
        if(p.is_absolute()&&fs::exists(p,ec))return p.wstring();
        if(fs::exists(p,ec))return fs::absolute(p,ec).wstring();
        wchar_t exe[MAX_PATH]{}; const DWORD n=GetModuleFileNameW(nullptr,exe,MAX_PATH); if(n==0)return configured;
        fs::path cursor=fs::path(exe).parent_path(); for(int i=0;i<7&&!cursor.empty();++i){fs::path candidate=cursor/p;if(fs::exists(candidate,ec))return candidate.wstring();cursor=cursor.parent_path();}
        return configured;
    }

    std::wstring EditorShellV3::Utf8ToWide_(const char* text)
    {
        if(!text||!*text)return{};
        const int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0);if(n<=1)return{};
        std::wstring out(static_cast<size_t>(n),L'\0');
        MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),n);
        out.resize(static_cast<size_t>(n-1));
        return out;
    }
}
