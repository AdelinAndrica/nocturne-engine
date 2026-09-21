#include "EditorShellV3Controls.h"

#include "EditorIconRenderer.h"
#include "EditorTheme.h"

#include <algorithm>
#include <new>
#include <utility>

#include <CommCtrl.h>
#include <Richedit.h>
#include <Windowsx.h>

namespace nocturne::editor::shellv3
{
    namespace
    {
        constexpr wchar_t kButtonClass[] = L"NocturneV3Button";
        constexpr wchar_t kHeaderClass[] = L"NocturneV3Header";
        constexpr wchar_t kTreeClass[] = L"NocturneV3Tree";
        constexpr wchar_t kTableClass[] = L"NocturneV3Table";
        constexpr wchar_t kScrollClass[] = L"NocturneV3Scroll";
        constexpr UINT WM_NOC_V3_ACTIVE = WM_APP + 0x310;
    }

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
struct TreeItem
{
    std::wstring text;
    int depth = 0;
    Icon icon = Icon::None;
    bool expandable = false;
    bool expanded = true;
    noc::EntityHandle entity{};
};
struct TreeState
{
    std::vector<TreeItem> items;

    // Design choice (not directly from the book): cache the projected
    // visible row indices so WM_PAINT / mouse-move / hit-testing do not
    // allocate temporary vectors on every event. The cache is rebuilt
    // only after structural or expand/collapse changes.
    std::vector<int> visibleItems;
    std::vector<uint8_t> expansionScratch;
    bool visibleDirty = true;

    HFONT font = nullptr;
    int firstRow = 0;
    int selected = -1;
    int hover = -1;

    POINT dragOrigin{};
    int dragSource = -1;
    int dropTarget = -1;
    bool dragging = false;
};
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

void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width)
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
    case WM_SETTEXT:
        if (state)
        {
            state->label =
                lParam
                    ? reinterpret_cast<const wchar_t*>(lParam)
                    : L"";
            InvalidateRect(hwnd, nullptr, FALSE);
            return TRUE;
        }
        break;
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

const std::vector<int>& VisibleTree(TreeState& state)
{
    if (!state.visibleDirty)
        return state.visibleItems;

    int maxDepth = 0;
    for (const TreeItem& item : state.items)
        maxDepth = (std::max)(maxDepth, item.depth);

    const std::size_t scratchSize =
        static_cast<std::size_t>(
            (std::max)(maxDepth + 1, 1));

    state.expansionScratch.resize(
        scratchSize,
        uint8_t{ 1 });
    std::fill(
        state.expansionScratch.begin(),
        state.expansionScratch.end(),
        uint8_t{ 1 });

    state.visibleItems.clear();
    if (state.visibleItems.capacity()
        < state.items.size())
    {
        state.visibleItems.reserve(
            state.items.size());
    }

    for (int i = 0;
         i < static_cast<int>(state.items.size());
         ++i)
    {
        const TreeItem& item =
            state.items[i];

        bool visible = true;

        for (int depth = 0;
             depth < item.depth
                && depth
                    < static_cast<int>(
                        state.expansionScratch.size());
             ++depth)
        {
            if (!state.expansionScratch[
                    static_cast<std::size_t>(depth)])
            {
                visible = false;
                break;
            }
        }

        if (visible)
            state.visibleItems.push_back(i);

        if (item.depth >= 0
            && item.depth
                < static_cast<int>(
                    state.expansionScratch.size()))
        {
            state.expansionScratch[
                static_cast<std::size_t>(
                    item.depth)] =
                static_cast<uint8_t>(
                    !item.expandable
                    || item.expanded);

            for (int depth = item.depth + 1;
                 depth
                    < static_cast<int>(
                        state.expansionScratch.size());
                 ++depth)
            {
                state.expansionScratch[
                    static_cast<std::size_t>(
                        depth)] = 1;
            }
        }
    }

    state.visibleDirty = false;
    return state.visibleItems;
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
            RECT rc{}; GetClientRect(hwnd, &rc); const auto& visible = VisibleTree(*state);
            const int page = (std::max)(1, static_cast<int>(rc.bottom) / rowH);
            const int maxFirst = (std::max)(0, static_cast<int>(visible.size()) - page);
            state->firstRow = (std::clamp)(state->firstRow - GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 3, 0, maxFirst);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state)
        {
            const auto& visible = VisibleTree(*state);
            const int visibleRow =
                state->firstRow
                + GET_Y_LPARAM(lParam) / rowH;
            const int hover =
                visibleRow >= 0
                && visibleRow < static_cast<int>(visible.size())
                    ? visible[visibleRow]
                    : -1;

            bool changed = false;

            if (hover != state->hover)
            {
                state->hover = hover;
                changed = true;
            }

            if ((wParam & MK_LBUTTON) != 0
                && state->dragSource >= 0)
            {
                const int dx =
                    GET_X_LPARAM(lParam)
                    - state->dragOrigin.x;
                const int dy =
                    GET_Y_LPARAM(lParam)
                    - state->dragOrigin.y;

                if (!state->dragging
                    && (std::abs(dx) >= 4
                        || std::abs(dy) >= 4))
                {
                    state->dragging = true;
                    SetCapture(hwnd);
                    changed = true;
                }

                if (state->dragging
                    && state->dropTarget != hover)
                {
                    state->dropTarget = hover;
                    changed = true;
                }
            }

            if (changed)
                InvalidateRect(hwnd, nullptr, FALSE);

            TRACKMOUSEEVENT t{
                sizeof(t),
                TME_LEAVE,
                hwnd,
                0
            };
            TrackMouseEvent(&t);
        }
        return 0;

    case WM_MOUSELEAVE:
        if (state && !state->dragging)
        {
            state->hover = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (state)
        {
            SetFocus(hwnd);

            const auto& visible =
                VisibleTree(*state);
            const int visibleRow =
                state->firstRow
                + GET_Y_LPARAM(lParam) / rowH;

            int index = 0;
            if (visibleRow >= 0
                && visibleRow
                    < static_cast<int>(visible.size()))
            {
                index = visible[visibleRow];
            }

            state->selected = index;
            state->dragSource = -1;
            state->dropTarget = -1;
            state->dragging = false;

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);

            SendMessageW(
                GetParent(hwnd),
                WM_COMMAND,
                MAKEWPARAM(
                    GetDlgCtrlID(hwnd),
                    kTreeSelectionChanged),
                reinterpret_cast<LPARAM>(hwnd));

            POINT screenPoint{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };
            ClientToScreen(
                hwnd,
                &screenPoint);

            SendMessageW(
                GetParent(hwnd),
                WM_NOC_V3_TREE_CONTEXT,
                static_cast<WPARAM>(
                    GetDlgCtrlID(hwnd)),
                reinterpret_cast<LPARAM>(
                    &screenPoint));
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (state)
        {
            SetFocus(hwnd);

            const auto& visible =
                VisibleTree(*state);
            const int visibleRow =
                state->firstRow
                + GET_Y_LPARAM(lParam) / rowH;

            state->dragSource = -1;
            state->dropTarget = -1;
            state->dragging = false;

            if (visibleRow >= 0
                && visibleRow
                    < static_cast<int>(visible.size()))
            {
                const int index =
                    visible[visibleRow];
                state->selected = index;

                const int arrowX =
                    9 + state->items[index].depth * 16;
                const bool arrowHit =
                    state->items[index].expandable
                    && GET_X_LPARAM(lParam) >= arrowX
                    && GET_X_LPARAM(lParam)
                        <= arrowX + 14;

                if (arrowHit)
                {
                    state->items[index].expanded =
                        !state->items[index].expanded;
                    state->visibleDirty = true;
                }
                else if (state->items[index].entity.IsValid())
                {
                    state->dragSource = index;
                    state->dragOrigin = {
                        GET_X_LPARAM(lParam),
                        GET_Y_LPARAM(lParam)
                    };
                }

                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);

                SendMessageW(
                    GetParent(hwnd),
                    WM_COMMAND,
                    MAKEWPARAM(
                        GetDlgCtrlID(hwnd),
                        kTreeSelectionChanged),
                    reinterpret_cast<LPARAM>(hwnd));
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (state)
        {
            const bool shouldDrop =
                state->dragging
                && state->dragSource >= 0
                && state->dropTarget >= 0
                && state->dragSource
                    < static_cast<int>(state->items.size())
                && state->dropTarget
                    < static_cast<int>(state->items.size());

            TreeReparentRequest request{};
            if (shouldDrop)
            {
                request.child =
                    state->items[
                        state->dragSource].entity;
                request.parent =
                    state->items[
                        state->dropTarget].entity;
            }

            state->dragSource = -1;
            state->dropTarget = -1;
            state->dragging = false;

            if (GetCapture() == hwnd)
                ReleaseCapture();

            if (shouldDrop)
            {
                SendMessageW(
                    GetParent(hwnd),
                    WM_NOC_V3_TREE_REPARENT,
                    static_cast<WPARAM>(
                        GetDlgCtrlID(hwnd)),
                    reinterpret_cast<LPARAM>(
                        &request));
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (state)
        {
            state->dragSource = -1;
            state->dropTarget = -1;
            state->dragging = false;
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc); const auto& c = EditorTheme::Colors(); Fill(dc, rc, c.panelBg);
        if (state)
        {
            const auto& visible = VisibleTree(*state);
            const int page = (std::max)(1, static_cast<int>(rc.bottom) / rowH);
            int y = 0;
            for (int v = state->firstRow; v < static_cast<int>(visible.size()) && y < rc.bottom; ++v, y += rowH)
            {
                const int index = visible[v]; const auto& item = state->items[index];
                RECT row{ 0, y, rc.right - 10, y + rowH };
                if (state->dragging
                    && index == state->dropTarget)
                {
                    Fill(
                        dc,
                        row,
                        Blend(c.panelBg, c.accent, 22));
                    Line(
                        dc,
                        static_cast<int>(row.left) + 2,
                        static_cast<int>(row.bottom) - 2,
                        static_cast<int>(row.right) - 2,
                        static_cast<int>(row.bottom) - 2,
                        c.accent,
                        2);
                }
                else if (index == state->selected)
                {
                    Fill(dc, row, Blend(c.panelBg, c.accent, 35));
                }
                else if (index == state->hover)
                {
                    Fill(dc, row, c.panelBgAlt);
                }
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
    HWND h = CreateWindowExW(0, kButtonClass, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS, 0,0,0,0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), &init);
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
void TreeClear(HWND h)
{
    auto* s = h
        ? reinterpret_cast<TreeState*>(
            GetWindowLongPtrW(h, GWLP_USERDATA))
        : nullptr;
    if (!s) return;

    if (GetCapture() == h)
        ReleaseCapture();

    s->items.clear();
    s->visibleItems.clear();
    s->visibleDirty = true;
    s->firstRow = 0;
    s->selected = -1;
    s->hover = -1;
    s->dragSource = -1;
    s->dropTarget = -1;
    s->dragging = false;
    InvalidateRect(h, nullptr, FALSE);
}
void TreeAdd(
    HWND h,
    std::wstring text,
    int depth,
    Icon icon,
    bool expandable,
    bool expanded,
    noc::EntityHandle entity)
{
    auto* s = h
        ? reinterpret_cast<TreeState*>(
            GetWindowLongPtrW(h, GWLP_USERDATA))
        : nullptr;
    if (!s) return;

    s->items.push_back({
        std::move(text),
        depth,
        icon,
        expandable,
        expanded,
        entity
    });
    s->visibleDirty = true;

    if (s->selected < 0)
        s->selected = 0;

    InvalidateRect(h, nullptr, FALSE);
}

void TreeCaptureExpansion(
    HWND h,
    std::vector<EditorHierarchyExpansionEntry>& outEntries,
    bool& outRootExpanded)
{
    outEntries.clear();
    outRootExpanded = true;

    auto* state = h
        ? reinterpret_cast<TreeState*>(
            GetWindowLongPtrW(h, GWLP_USERDATA))
        : nullptr;
    if (!state)
        return;

    if (!state->items.empty())
        outRootExpanded = state->items[0].expanded;

    try
    {
        outEntries.reserve(state->items.size());

        for (const TreeItem& item : state->items)
        {
            if (!item.entity.IsValid())
                continue;

            outEntries.push_back({
                item.entity,
                item.expanded
            });
        }
    }
    catch (const std::bad_alloc&)
    {
        outEntries.clear();
        outRootExpanded = true;
    }
}

[[nodiscard]] noc::EntityHandle TreeSelectedEntity(HWND h)
{
    auto* s = h
        ? reinterpret_cast<TreeState*>(
            GetWindowLongPtrW(h, GWLP_USERDATA))
        : nullptr;

    if (!s
        || s->selected < 0
        || s->selected >= static_cast<int>(s->items.size()))
    {
        return noc::EntityHandle::Invalid();
    }

    return s->items[s->selected].entity;
}

void TreeSelectEntity(
    HWND h,
    noc::EntityHandle entity)
{
    auto* s = h
        ? reinterpret_cast<TreeState*>(
            GetWindowLongPtrW(h, GWLP_USERDATA))
        : nullptr;
    if (!s) return;

    int selected = 0;
    if (entity.IsValid())
    {
        for (int i = 0;
             i < static_cast<int>(s->items.size());
             ++i)
        {
            if (s->items[i].entity == entity)
            {
                selected = i;
                break;
            }
        }
    }

    if (s->selected != selected)
    {
        s->selected = selected;
        InvalidateRect(h, nullptr, FALSE);
    }
}
bool TreeSelectedLabelRect(
    HWND h,
    RECT& outRect)
{
    auto* state = h
        ? reinterpret_cast<TreeState*>(
            GetWindowLongPtrW(h, GWLP_USERDATA))
        : nullptr;

    if (!state
        || state->selected < 0
        || state->selected
            >= static_cast<int>(state->items.size()))
    {
        return false;
    }

    const auto& visible = VisibleTree(*state);
    int visibleIndex = -1;

    for (int i = 0;
         i < static_cast<int>(visible.size());
         ++i)
    {
        if (visible[i] == state->selected)
        {
            visibleIndex = i;
            break;
        }
    }

    if (visibleIndex < 0)
        return false;

    constexpr int rowHeight = 24;
    const int localRow =
        visibleIndex - state->firstRow;

    RECT client{};
    GetClientRect(h, &client);

    if (localRow < 0
        || localRow * rowHeight >= client.bottom)
    {
        return false;
    }

    const TreeItem& item =
        state->items[state->selected];
    const int arrowX =
        9 + item.depth * 16;

    outRect = {
        arrowX + 32,
        localRow * rowHeight + 2,
        (std::max)(
            arrowX + 92,
            static_cast<int>(client.right) - 11),
        localRow * rowHeight + rowHeight - 2
    };
    return true;
}

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
