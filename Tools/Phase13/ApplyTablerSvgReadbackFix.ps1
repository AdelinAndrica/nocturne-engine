$ErrorActionPreference = 'Stop'

$path = Join-Path $PSScriptRoot '..\..\Apps\NocturneEditor\EditorIconRenderer.cpp'
$path = [System.IO.Path]::GetFullPath($path)

if (-not (Test-Path $path)) {
    throw "EditorIconRenderer.cpp not found: $path"
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false, $true)
$content = [System.IO.File]::ReadAllText($path, $utf8NoBom)

# Match only the Rasterize() function body. Do not depend on LF vs CRLF;
# the source may have either line ending locally.
$pattern = '(?s)            HBITMAP Rasterize\(EditorIconId icon, int width, int height, COLORREF color\)\s*\{.*?\r?\n            \}(?=\r?\n\r?\n        private:)'

$replacement = @'
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

                ComPtr<ID2D1SvgDocument> document;
                hr = svgContext_->CreateSvgDocument(stream.Get(),
                    D2D1::SizeF(static_cast<float>(width), static_cast<float>(height)), &document);
                if (FAILED(hr)) return nullptr;

                svgContext_->SetTarget(target.Get());
                svgContext_->BeginDraw();
                svgContext_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
                svgContext_->DrawSvgDocument(document.Get());
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
'@

$patched = [regex]::Replace($content, $pattern, $replacement, 1)
if ($patched -eq $content) {
    throw 'Could not locate Rasterize() in EditorIconRenderer.cpp.'
}

[System.IO.File]::WriteAllText($path, $patched, $utf8NoBom)
Write-Host "Patched: $path"
Write-Host 'Review with: git diff -- Apps/NocturneEditor/EditorIconRenderer.cpp'
