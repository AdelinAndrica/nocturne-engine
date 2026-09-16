#include "EditorIconRenderer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "D3d11.lib")
#pragma comment(lib, "Windowscodecs.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Msimg32.lib")

namespace nocturne::editor
{
    namespace
    {
        using Microsoft::WRL::ComPtr;
        namespace fs = std::filesystem;

        struct CacheKey
        {
            int icon = 0;
            int width = 0;
            int height = 0;
            COLORREF color = 0;

            bool operator==(const CacheKey& rhs) const noexcept
            {
                return icon == rhs.icon && width == rhs.width && height == rhs.height && color == rhs.color;
            }
        };

        struct CacheKeyHash
        {
            size_t operator()(const CacheKey& key) const noexcept
            {
                size_t h = static_cast<size_t>(key.icon);
                h = (h * 1315423911u) ^ static_cast<size_t>(key.width);
                h = (h * 1315423911u) ^ static_cast<size_t>(key.height);
                h = (h * 1315423911u) ^ static_cast<size_t>(key.color);
                return h;
            }
        };

        const wchar_t* FileNameFor(EditorIconId icon)
        {
            switch (icon)
            {
            case EditorIconId::Document:  return L"file.svg";
            case EditorIconId::Folder:    return L"folder.svg";
            case EditorIconId::Save:      return L"device-floppy.svg";
            case EditorIconId::Undo:      return L"arrow-back-up.svg";
            case EditorIconId::Redo:      return L"arrow-forward-up.svg";
            case EditorIconId::Cursor:    return L"pointer.svg";
            case EditorIconId::Move:      return L"arrows-move.svg";
            case EditorIconId::Rotate:    return L"rotate.svg";
            case EditorIconId::Scale:     return L"maximize.svg";
            case EditorIconId::Play:      return L"player-play.svg";
            case EditorIconId::Stop:      return L"player-stop.svg";
            case EditorIconId::Cube:      return L"box.svg";
            case EditorIconId::List:      return L"list.svg";
            case EditorIconId::Grid:      return L"layout-grid.svg";
            case EditorIconId::Settings:  return L"settings.svg";
            case EditorIconId::Hierarchy: return L"hierarchy-2.svg";
            case EditorIconId::Viewport:  return L"device-desktop.svg";
            case EditorIconId::Inspector: return L"adjustments-horizontal.svg";
            case EditorIconId::Console:   return L"terminal-2.svg";
            case EditorIconId::World:     return L"world.svg";
            case EditorIconId::Camera:    return L"camera.svg";
            case EditorIconId::Mesh:      return L"box.svg";
            case EditorIconId::Texture:   return L"photo.svg";
            case EditorIconId::Material:  return L"sphere.svg";
            case EditorIconId::Text:      return L"file-text.svg";
            case EditorIconId::Metadata:  return L"braces.svg";
            default:                      return nullptr;
            }
        }

        fs::path FindIconRoot()
        {
            std::error_code ec;
            const auto test = [&](const fs::path& base) -> fs::path
            {
                if (base.empty()) return {};
                fs::path candidate = base / L"ThirdParty" / L"TablerIcons" / L"icons" / L"outline";
                if (fs::exists(candidate / L"file.svg", ec)) return candidate;
                return {};
            };

            fs::path cwd = fs::current_path(ec);
            if (auto found = test(cwd); !found.empty()) return found;

            wchar_t modulePath[MAX_PATH]{};
            const DWORD count = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
            if (count == 0) return {};

            fs::path cursor = fs::path(modulePath).parent_path();
            for (int i = 0; i < 8 && !cursor.empty(); ++i)
            {
                if (auto found = test(cursor); !found.empty()) return found;
                cursor = cursor.parent_path();
            }
            return {};
        }

        std::string LoadTextFile(const fs::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) return {};
            file.seekg(0, std::ios::end);
            const std::streamoff size = file.tellg();
            if (size <= 0) return {};
            file.seekg(0, std::ios::beg);
            std::string text(static_cast<size_t>(size), '\0');
            file.read(text.data(), size);
            return file ? text : std::string{};
        }

        std::string TintSvg(std::string svg, COLORREF color)
        {
            char hex[8]{};
            std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
            constexpr const char* token = "currentColor";
            size_t pos = 0;
            while ((pos = svg.find(token, pos)) != std::string::npos)
            {
                svg.replace(pos, std::strlen(token), hex);
                pos += 7;
            }
            return svg;
        }

        class IconRenderer final
        {
        public:
            ~IconRenderer() { Shutdown(); }

            bool Draw(HDC dc, EditorIconId icon, const RECT& rect, COLORREF color)
            {
                if (!dc || icon == EditorIconId::None) return false;

                const int slotWidth = static_cast<int>(rect.right - rect.left);
                const int slotHeight = static_cast<int>(rect.bottom - rect.top);
                if (slotWidth <= 0 || slotHeight <= 0) return false;

                // Design choice (not directly from the book): Tabler's 24x24 / 2px
                // outline reads too heavy when it fills our compact Win32 icon slot.
                // Keep the layout slot unchanged, but render a smaller glyph centered
                // inside it: 12px for toolbar-style 16px slots and 11px for the
                // 14-15px hierarchy/header/table slots.
                const int slotMin = (std::min)(slotWidth, slotHeight);
                const int glyphSize = slotMin >= 16 ? 12 : slotMin >= 14 ? 11 : slotMin;
                if (glyphSize <= 0) return false;

                const int drawX = rect.left + (slotWidth - glyphSize) / 2;
                const int drawY = rect.top + (slotHeight - glyphSize) / 2;

                std::lock_guard<std::mutex> lock(mutex_);
                if (!EnsureInitialized()) return false;

                const CacheKey key{ static_cast<int>(icon), glyphSize, glyphSize, color };
                auto it = cache_.find(key);
                HBITMAP bitmap = it == cache_.end() ? nullptr : it->second;
                if (!bitmap)
                {
                    bitmap = Rasterize(icon, glyphSize, glyphSize, color);
                    if (!bitmap) return false;
                    cache_.emplace(key, bitmap);
                }

                HDC memory = CreateCompatibleDC(dc);
                if (!memory) return false;
                HGDIOBJ old = SelectObject(memory, bitmap);
                BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                const BOOL ok = AlphaBlend(dc, drawX, drawY, glyphSize, glyphSize,
                    memory, 0, 0, glyphSize, glyphSize, blend);
                SelectObject(memory, old);
                DeleteDC(memory);
                return ok != FALSE;
            }

            void Shutdown()
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& entry : cache_)
                {
                    if (entry.second) DeleteObject(entry.second);
                }
                cache_.clear();
                svgContext_.Reset();
                d2dContext_.Reset();
                d2dDevice_.Reset();
                d2dFactory_.Reset();
                d3dContext_.Reset();
                d3dDevice_.Reset();
                wicFactory_.Reset();
                iconRoot_.clear();
                usable_ = false;
                initialized_ = false;
                if (ownsCom_)
                {
                    CoUninitialize();
                    ownsCom_ = false;
                }
            }

        private:
            bool EnsureInitialized()
            {
                if (initialized_) return usable_;
                initialized_ = true;

                iconRoot_ = FindIconRoot();
                if (iconRoot_.empty()) return false;

                const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (SUCCEEDED(comHr)) ownsCom_ = true;
                else if (comHr != RPC_E_CHANGED_MODE) return false;

                HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&wicFactory_));
                if (FAILED(hr))
                {
                    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&wicFactory_));
                }
                if (FAILED(hr)) return false;

                UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
                D3D_FEATURE_LEVEL requested[] = { D3D_FEATURE_LEVEL_11_0 };
                D3D_FEATURE_LEVEL created{};
                hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                    requested, 1, D3D11_SDK_VERSION, &d3dDevice_, &created, &d3dContext_);
                if (FAILED(hr))
                {
                    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                        requested, 1, D3D11_SDK_VERSION, &d3dDevice_, &created, &d3dContext_);
                }
                if (FAILED(hr)) return false;

                ComPtr<IDXGIDevice> dxgiDevice;
                hr = d3dDevice_.As(&dxgiDevice);
                if (FAILED(hr)) return false;

                D2D1_FACTORY_OPTIONS options{};
                hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                    __uuidof(ID2D1Factory1), &options,
                    reinterpret_cast<void**>(d2dFactory_.ReleaseAndGetAddressOf()));
                if (FAILED(hr)) return false;

                hr = d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_);
                if (FAILED(hr)) return false;
                hr = d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_);
                if (FAILED(hr)) return false;
                hr = d2dContext_.As(&svgContext_);
                if (FAILED(hr)) return false;

                usable_ = true;
                return true;
            }

            HBITMAP Rasterize(EditorIconId icon, int width, int height, COLORREF color)
            {
                const wchar_t* fileName = FileNameFor(icon);
                if (!fileName) return nullptr;

                std::string svg = LoadTextFile(iconRoot_ / fileName);
                if (svg.empty()) return nullptr;
                svg = TintSvg(std::move(svg), color);

                D2D1_BITMAP_PROPERTIES1 targetProperties = D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_TARGET,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                    96.0f, 96.0f);

                ComPtr<ID2D1Bitmap1> target;
                HRESULT hr = d2dContext_->CreateBitmap(
                    D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)),
                    nullptr, 0, &targetProperties, &target);
                if (FAILED(hr)) return nullptr;

                ComPtr<IStream> stream;
                stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()),
                    static_cast<UINT>(svg.size())));
                if (!stream) return nullptr;

                constexpr float kTablerLogicalSize = 24.0f;

                ComPtr<ID2D1SvgDocument> document;
                hr = svgContext_->CreateSvgDocument(
                    stream.Get(),
                    D2D1::SizeF(kTablerLogicalSize, kTablerLogicalSize),
                    &document);
                if (FAILED(hr)) return nullptr;

                const float scaleX = static_cast<float>(width) / kTablerLogicalSize;
                const float scaleY = static_cast<float>(height) / kTablerLogicalSize;

                D2D1_MATRIX_3X2_F previousTransform{};
                svgContext_->GetTransform(&previousTransform);

                svgContext_->SetTarget(target.Get());
                svgContext_->BeginDraw();
                svgContext_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
                svgContext_->SetTransform(D2D1::Matrix3x2F::Scale(scaleX, scaleY));
                svgContext_->DrawSvgDocument(document.Get());
                svgContext_->SetTransform(previousTransform);
                hr = svgContext_->EndDraw();
                svgContext_->SetTarget(nullptr);
                document.Reset();
                if (FAILED(hr)) return nullptr;

                // CreateBitmapFromWicBitmap does not give us a shared writable backing
                // store for readback. Copy the rendered D2D target into a CPU-readable
                // staging bitmap instead, then map those pixels explicitly.
                D2D1_BITMAP_PROPERTIES1 readbackProperties = D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                    96.0f, 96.0f);

                ComPtr<ID2D1Bitmap1> readback;
                hr = d2dContext_->CreateBitmap(
                    D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)),
                    nullptr, 0, &readbackProperties, &readback);
                if (FAILED(hr)) return nullptr;

                hr = readback->CopyFromBitmap(nullptr, target.Get(), nullptr);
                target.Reset();
                if (FAILED(hr)) return nullptr;

                D2D1_MAPPED_RECT mapped{};
                hr = readback->Map(D2D1_MAP_OPTIONS_READ, &mapped);
                if (FAILED(hr) || !mapped.bits) return nullptr;

                bool hasVisiblePixel = false;
                for (int y = 0; y < height && !hasVisiblePixel; ++y)
                {
                    const BYTE* row = mapped.bits + static_cast<size_t>(y) * mapped.pitch;
                    for (int x = 0; x < width; ++x)
                    {
                        if (row[static_cast<size_t>(x) * 4u + 3u] != 0)
                        {
                            hasVisiblePixel = true;
                            break;
                        }
                    }
                }

                if (!hasVisiblePixel)
                {
                    readback->Unmap();
                    return nullptr;
                }

                BITMAPINFO info{};
                info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth = width;
                info.bmiHeader.biHeight = -height;
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 32;
                info.bmiHeader.biCompression = BI_RGB;

                void* destination = nullptr;
                HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS,
                    &destination, nullptr, 0);
                if (!bitmap || !destination)
                {
                    if (bitmap) DeleteObject(bitmap);
                    readback->Unmap();
                    return nullptr;
                }

                const size_t rowBytes = static_cast<size_t>(width) * 4u;
                auto* dst = static_cast<BYTE*>(destination);
                for (int y = 0; y < height; ++y)
                {
                    std::memcpy(dst + static_cast<size_t>(y) * rowBytes,
                        mapped.bits + static_cast<size_t>(y) * mapped.pitch,
                        rowBytes);
                }

                readback->Unmap();
                return bitmap;
            }

        private:
            std::mutex mutex_;
            bool initialized_ = false;
            bool usable_ = false;
            bool ownsCom_ = false;
            fs::path iconRoot_;
            std::unordered_map<CacheKey, HBITMAP, CacheKeyHash> cache_;

            ComPtr<IWICImagingFactory> wicFactory_;
            ComPtr<ID3D11Device> d3dDevice_;
            ComPtr<ID3D11DeviceContext> d3dContext_;
            ComPtr<ID2D1Factory1> d2dFactory_;
            ComPtr<ID2D1Device> d2dDevice_;
            ComPtr<ID2D1DeviceContext> d2dContext_;
            ComPtr<ID2D1DeviceContext5> svgContext_;
        };

        IconRenderer& Renderer()
        {
            static IconRenderer renderer;
            return renderer;
        }
    }

    bool DrawEditorSvgIcon(HDC dc, EditorIconId icon, const RECT& rect, COLORREF color)
    {
        return Renderer().Draw(dc, icon, rect, color);
    }

    void ShutdownEditorIconRenderer()
    {
        Renderer().Shutdown();
    }
}
